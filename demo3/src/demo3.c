/*
 * Local Control 2 (Intersection 2) - QNET INPUT VERSION (VM8)
 *
 * STYLE MATCHES Local1:
 * - NORMAL is table-driven (array of states)
 * - TRAIN is its own mini-FSM (prints TRAIN S01..)
 *
 * PED RULE (UPDATED):
 * - Press 'p' arms a request.
 * - PED becomes WALK ONLY at the next SAFE ALL-RED.
 * - PED stays WALK during:
 *      (1) that SAFE ALL-RED
 *      (2) the immediately following PRE-Y
 * - Then PED returns to RED and prints *** PED OVER ***
 *
 * Local2 difference:
 * - R3 has ONLY ONE direction at this intersection (N-S)
 * - Side road is R2 (two directions W->E and E->W)
 *
 * QNET server:
 * - name_attach at: /dev/name/local/traffic_evt   (VM8)
 * - VM7 client sends MsgSend to: /net/<vm8_node>/dev/name/local/traffic_evt
 *
 * Events:
 *   't' = Train detected  (preempt if in NORMAL green)
 *   'c' = Train cleared   (exit TRAIN at next SAFE all-red)
 *   'p' = Ped button      (PED active only during SAFE ALL-RED + next PRE-Y)
 *
 * TRAIN SEQUENCE (NO SRL, print S01..S08):
 *   S01 (02s) R3=PRE-Y, R2=RED/RED
 *   S02 (08s) R3=LR-G,  R2=RED/RED
 *   S03 (04s) R3=LR-Y,  R2=RED/RED
 *   S04 (02s) ALL RED
 *   S05 (02s) R2=PRE-Y/PRE-Y (R3 RED)
 *   S06 (15s) R2(W->E)=SL-G, R2(E->W)=SR-G (R3 RED)
 *   S07 (04s) R2(W->E)=SL-Y, R2(E->W)=SR-Y (R3 RED)
 *   S08 (02s) ALL RED
 *   loop back to S01 while train_active && !train_clear_pending
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <sys/dispatch.h>
#include <sys/neutrino.h>

/* ================= QNET CONFIG ================= */
#define ATTACH_POINT "traffic_evt"

/* ================= EVENTS ================= */
#define EVT_TRAIN_DETECT  't'
#define EVT_TRAIN_CLEAR   'c'
#define EVT_PED_PRESS     'p'

/* ================= TIMINGS (seconds) =================
 * NORMAL (shared names with Local1)
 */
#define T_RS_GREEN   20
#define T_L_GREEN    12
#define T_YELLOW      4
#define T_ALL_RED     2
#define T_PREP_Y      2   /* PRE-Y inserted so S02 = PRE-Y */

/* TRAIN (shared names with Local1) */
#define T_TR_2_G      8
#define T_TR_3_G     15
#define T_TR_Y        4
#define T_TR_R        2
#define T_TR_PREP_Y   2

/* ================= QNET MESSAGE LAYOUT =================
 * MUST match VM7 client structs exactly.
 */
typedef struct {
    _Uint16t type;
    _Uint16t subtype;
    char     ev;        /* 't','c','p' */
    char     pad[3];
    int      client_id;
} evt_msg_t;

typedef struct {
    _Uint16t type;
    _Uint16t subtype;
    char     text[64];
} evt_reply_t;

/* QNET server handle */
static name_attach_t *g_attach = NULL;

/* ================= FLAGS ================= */
static int train_request = 0;        /* set by 't'; consumed when entering TRAIN */
static int train_active  = 0;        /* train currently present */
static int train_clear_pending = 0;  /* set by 'c'; exit at SAFE all-red */
static int ped_request   = 0;        /* set by 'p'; starts PED window at SAFE all-red */

static int in_train_state = 0;       /* 0=NORMAL, 1=TRAIN */
static int train_preempt_to_allred = 0;

/* ================= UI (Local1 style) ================= */
static unsigned train_ui_step = 1;   /* TRAIN prints S01.. */
static int normal_ui_base = 0;       /* NORMAL shift so chosen start prints S01 */

/* ================= PED WINDOW (ALL-RED + next PRE-Y) ================= */
static int ped_window_active = 0;          /* if 1 => PED=WALK, else RED */
static int ped_window_stop_after_prep = 0; /* if 1 => stop PED at end of next PRE-Y */

/* ================= NOTIFY ================= */
static void notify_train_begin(void)   { printf("\n*** TRAIN BEGIN ***\n\n"); fflush(stdout); }
static void notify_train_over(void)    { printf("\n*** TRAIN OVER  ***\n\n"); fflush(stdout); }
static void notify_train_preempt(void) { printf("\n>>> TRAIN PREEMPT: forcing YELLOW immediately <<<\n\n"); fflush(stdout); }
static void notify_train_clear(void)   { printf("\n>>> TRAIN CLEAR: will exit at next SAFE ALL-RED <<<\n\n"); fflush(stdout); }
static void notify_ped_begin(void)     { printf("\n*** PED BEGIN   ***\n\n"); fflush(stdout); }
static void notify_ped_over(void)      { printf("\n*** PED OVER    ***\n\n"); fflush(stdout); }

/* =========================================================
   PED output
   ========================================================= */
static const char* ped_output(void)
{
    return ped_window_active ? "WALK" : "RED";
}

/* Start PED window ONLY at SAFE ALL-RED (consumes ped_request) */
static void ped_try_start_at_safe_allred(int is_safe_allred_now)
{
    if (!is_safe_allred_now) return;
    if (!ped_request) return;

    ped_request = 0;
    ped_window_active = 1;
    ped_window_stop_after_prep = 1;
    notify_ped_begin();
}

/* Stop PED window at END of PRE-Y */
static void ped_stop_if_prep_finished(void)
{
    if (ped_window_active && ped_window_stop_after_prep) {
        ped_window_active = 0;
        ped_window_stop_after_prep = 0;
        notify_ped_over();
    }
}

/* =========================================================
   QNET INPUT: poll events without blocking
   ========================================================= */
static void poll_events_from_qnet_nonblock(void)
{
    if (!g_attach) return;

    evt_msg_t msg;
    evt_reply_t rep;
    memset(&msg, 0, sizeof(msg));
    memset(&rep, 0, sizeof(rep));
    rep.type = 0x01;
    rep.subtype = 0;

    _Uint64t timeout_ns = 0; /* immediate */
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &timeout_ns, NULL);

    int rcvid = MsgReceive(g_attach->chid, &msg, sizeof(msg), NULL);
    if (rcvid == -1) {
        if (errno == ETIMEDOUT) return;
        perror("MsgReceive");
        return;
    }

    if (msg.type == _IO_CONNECT) {
        MsgReply(rcvid, EOK, NULL, 0);
        return;
    }

    if (msg.type > _IO_BASE && msg.type <= _IO_MAX) {
        MsgError(rcvid, ENOSYS);
        return;
    }

    char ev = msg.ev;

    if (ev == EVT_TRAIN_DETECT) {
        notify_train_preempt();      /* print every press (matches your style) */
        train_request = 1;
        train_active  = 1;
        train_clear_pending = 0;
        snprintf(rep.text, sizeof(rep.text), "OK: t");

    } else if (ev == EVT_TRAIN_CLEAR) {
        train_clear_pending = 1;
        train_request = 0;
        snprintf(rep.text, sizeof(rep.text), "OK: c");

    } else if (ev == EVT_PED_PRESS) {
        ped_request = 1; /* arms only; starts at next SAFE ALL-RED */
        snprintf(rep.text, sizeof(rep.text), "OK: p");

    } else {
        snprintf(rep.text, sizeof(rep.text), "IGNORED");
    }

    MsgReply(rcvid, EOK, &rep, sizeof(rep));
}

/* ================= WAIT helper (100ms polling) ================= */
static void wait_with_poll_ms(unsigned total_ms)
{
    const long step_ns = 100L * 1000L * 1000L;
    unsigned steps = total_ms / 100U;
    unsigned rem   = total_ms % 100U;

    struct timespec ts = { .tv_sec = 0, .tv_nsec = step_ns };

    for (unsigned i = 0; i < steps; i++) {
        nanosleep(&ts, NULL);
        poll_events_from_qnet_nonblock();
    }

    if (rem) {
        struct timespec ts2 = { .tv_sec = 0, .tv_nsec = (long)rem * 1000L * 1000L };
        nanosleep(&ts2, NULL);
        poll_events_from_qnet_nonblock();
    }
}

/* =========================================================
   NORMAL (table-driven)
   S01=ALL-RED, S02=PRE-Y
   ========================================================= */

typedef enum {
    N_ALL_RED_1 = 0,
    N_PREP_R3,
    N_R3_RS_G,
    N_R3_RS_Y,
    N_R3_L_G,
    N_R3_L_Y,
    N_ALL_RED_2,
    N_PREP_R2,
    N_R2_RS_G,
    N_R2_RS_Y,
    N_R2_L_G,
    N_R2_L_Y
} normal_state_t;

typedef struct {
    normal_state_t id;
    unsigned dur_s;
    const char *r3_ns;
    const char *r2_we;
    const char *r2_ew;
    int  is_safe_allred;
    int  is_prep_y;
    int  is_green;
    normal_state_t to_yellow;
    normal_state_t next;
} normal_def_t;

static const normal_def_t NORM[] = {
    /* S01 */ { N_ALL_RED_1, T_ALL_RED, "RED",   "RED",   "RED",   1, 0, 0, N_ALL_RED_1, N_PREP_R3 },
    /* S02 */ { N_PREP_R3,   T_PREP_Y,  "PRE-Y", "RED",   "RED",   0, 1, 0, N_PREP_R3,   N_R3_RS_G },

    { N_R3_RS_G,  T_RS_GREEN, "RS-G", "RED", "RED", 0, 0, 1, N_R3_RS_Y, N_R3_RS_Y },
    { N_R3_RS_Y,  T_YELLOW,   "RS-Y", "RED", "RED", 0, 0, 0, N_R3_RS_Y, N_R3_L_G  },
    { N_R3_L_G,   T_L_GREEN,  "L-G",  "RED", "RED", 0, 0, 1, N_R3_L_Y,  N_R3_L_Y  },
    { N_R3_L_Y,   T_YELLOW,   "L-Y",  "RED", "RED", 0, 0, 0, N_R3_L_Y,  N_ALL_RED_2 },

    { N_ALL_RED_2, T_ALL_RED, "RED", "RED", "RED", 1, 0, 0, N_ALL_RED_2, N_PREP_R2 },
    { N_PREP_R2,   T_PREP_Y,  "RED", "PRE-Y","PRE-Y", 0, 1, 0, N_PREP_R2, N_R2_RS_G },

    { N_R2_RS_G,  T_RS_GREEN, "RED", "RS-G", "RS-G", 0, 0, 1, N_R2_RS_Y, N_R2_RS_Y },
    { N_R2_RS_Y,  T_YELLOW,   "RED", "RS-Y", "RS-Y", 0, 0, 0, N_R2_RS_Y, N_R2_L_G  },
    { N_R2_L_G,   T_L_GREEN,  "RED", "L-G",  "L-G",  0, 0, 1, N_R2_L_Y,  N_R2_L_Y  },
    { N_R2_L_Y,   T_YELLOW,   "RED", "L-Y",  "L-Y",  0, 0, 0, N_R2_L_Y,  N_ALL_RED_1 }
};

static const normal_def_t* find_norm(normal_state_t id)
{
    for (unsigned i = 0; i < sizeof(NORM)/sizeof(NORM[0]); i++) {
        if (NORM[i].id == id) return &NORM[i];
    }
    return NULL;
}

static int normal_ui_index_raw(normal_state_t s)
{
    for (unsigned i = 0; i < sizeof(NORM)/sizeof(NORM[0]); i++) {
        if (NORM[i].id == s) return (int)i + 1;
    }
    return 1;
}

static void normal_ui_set_start(normal_state_t start_state)
{
    int raw = normal_ui_index_raw(start_state);
    normal_ui_base = raw - 1;
}

static int normal_ui_index_shifted(normal_state_t s)
{
    int raw = normal_ui_index_raw(s);
    int shifted = raw - normal_ui_base;
    int total = (int)(sizeof(NORM)/sizeof(NORM[0]));
    if (shifted <= 0) shifted += total;
    return shifted;
}

static int is_normal_green(normal_state_t s)
{
    const normal_def_t *st = find_norm(s);
    return (st && st->is_green);
}

/* =========================================================
   TRAIN mini-FSM (prints TRAIN S01..)
   (NO SRL) exactly as requested
   ========================================================= */

typedef enum {
    TR_S01_PREP_R3 = 0,
    TR_S02_R3_LR_G,
    TR_S03_R3_LR_Y,
    TR_S04_ALL_RED_A,
    TR_S05_PREP_R2,
    TR_S06_R2_SPLIT_G,
    TR_S07_R2_SPLIT_Y,
    TR_S08_ALL_RED_B
} train_state_t;

static train_state_t tr = TR_S01_PREP_R3;

static int train_is_safe_allred(train_state_t s)
{
    return (s == TR_S04_ALL_RED_A || s == TR_S08_ALL_RED_B);
}

static unsigned train_duration(train_state_t s)
{
    switch (s) {
        case TR_S01_PREP_R3:    return T_TR_PREP_Y;  /* 2s */
        case TR_S02_R3_LR_G:    return T_TR_2_G;     /* 8s */
        case TR_S03_R3_LR_Y:    return T_TR_Y;       /* 4s */
        case TR_S04_ALL_RED_A:  return T_TR_R;       /* 2s */
        case TR_S05_PREP_R2:    return T_TR_PREP_Y;  /* 2s */
        case TR_S06_R2_SPLIT_G: return T_TR_3_G;     /* 15s */
        case TR_S07_R2_SPLIT_Y: return T_TR_Y;       /* 4s */
        case TR_S08_ALL_RED_B:  return T_TR_R;       /* 2s */
        default: return 1;
    }
}

static unsigned train_ui_label(train_state_t s)
{
    /* print S01..S08 exactly */
    return (unsigned)s + 1U;
}

static int train_is_prep(train_state_t s)
{
    return (s == TR_S01_PREP_R3 || s == TR_S05_PREP_R2);
}

static void print_train_line(train_state_t s)
{
    const char *r3   = "RED";
    const char *r2we = "RED";
    const char *r2ew = "RED";

    switch (s) {
        case TR_S01_PREP_R3:
            r3 = "PRE-Y";
            break;

        case TR_S02_R3_LR_G:
            r3 = "LR-G";
            break;

        case TR_S03_R3_LR_Y:
            r3 = "LR-Y";
            break;

        case TR_S05_PREP_R2:
            r2we = "PRE-Y";
            r2ew = "PRE-Y";
            break;

        case TR_S06_R2_SPLIT_G:
            r2we = "SL-G";
            r2ew = "SR-G";
            break;

        case TR_S07_R2_SPLIT_Y:
            r2we = "SL-Y";
            r2ew = "SR-Y";
            break;

        default:
            /* ALL-RED states => all RED */
            break;
    }

    printf("[TRAIN  S%02u] (%02us) | R3(N-S)=%-6s | R2(W->E)=%-6s | R2(E->W)=%-6s | PED=%-5s\n",
           train_ui_label(s), train_duration(s),
           r3, r2we, r2ew, ped_output());
    fflush(stdout);
}

static void train_step(void)
{
    /* If 'p' was pressed, PED starts ONLY when we are at SAFE ALL-RED */
    ped_try_start_at_safe_allred(train_is_safe_allred(tr));

    print_train_line(tr);
    wait_with_poll_ms(train_duration(tr) * 1000U);

    /* If we just finished a PRE-Y, stop PED now */
    if (train_is_prep(tr)) {
        ped_stop_if_prep_finished();
    }

    switch (tr) {
        case TR_S01_PREP_R3:    tr = TR_S02_R3_LR_G;    break;
        case TR_S02_R3_LR_G:    tr = TR_S03_R3_LR_Y;    break;
        case TR_S03_R3_LR_Y:    tr = TR_S04_ALL_RED_A;  break;
        case TR_S04_ALL_RED_A:  tr = TR_S05_PREP_R2;    break;
        case TR_S05_PREP_R2:    tr = TR_S06_R2_SPLIT_G; break;
        case TR_S06_R2_SPLIT_G: tr = TR_S07_R2_SPLIT_Y; break;
        case TR_S07_R2_SPLIT_Y: tr = TR_S08_ALL_RED_B;  break;

        case TR_S08_ALL_RED_B:
            if (train_active && !train_clear_pending) {
                tr = TR_S01_PREP_R3; /* loop */
            } else {
                tr = TR_S04_ALL_RED_A; /* hold safe red-ish (doesn't matter; exit handled elsewhere) */
            }
            break;

        default:
            tr = TR_S01_PREP_R3;
            break;
    }
}

/* =========================================================
   NORMAL print + step
   ========================================================= */
static void print_normal_line(normal_state_t s, const normal_def_t *st)
{
    printf("[NORMAL S%02d] (%02us) | R3(N-S)=%-6s | R2(W->E)=%-6s | R2(E->W)=%-6s | PED=%-5s\n",
           normal_ui_index_shifted(s), st->dur_s,
           st->r3_ns, st->r2_we, st->r2_ew, ped_output());
    fflush(stdout);
}

static void normal_step(normal_state_t *cur)
{
    const normal_def_t *st = find_norm(*cur);
    if (!st) { *cur = N_ALL_RED_1; return; }

    /* If 'p' was pressed, PED starts ONLY at SAFE ALL-RED */
    ped_try_start_at_safe_allred(st->is_safe_allred);

    print_normal_line(*cur, st);

    /* Wait in 100ms ticks (poll only) */
    unsigned total_ms = st->dur_s * 1000U;
    const long step_ns = 100L * 1000L * 1000L;
    unsigned steps = total_ms / 100U;
    unsigned rem   = total_ms % 100U;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = step_ns };

    for (unsigned i = 0; i < steps; i++) {
        nanosleep(&ts, NULL);
        poll_events_from_qnet_nonblock();

        /* PREEMPT: if train requested while in NORMAL GREEN => force YELLOW immediately */
        if (train_request && is_normal_green(*cur)) {
            train_preempt_to_allred = 1;
            *cur = st->to_yellow;
            return;
        }
    }
    if (rem) {
        struct timespec ts2 = { .tv_sec = 0, .tv_nsec = (long)rem * 1000L * 1000L };
        nanosleep(&ts2, NULL);
        poll_events_from_qnet_nonblock();

        if (train_request && is_normal_green(*cur)) {
            train_preempt_to_allred = 1;
            *cur = st->to_yellow;
            return;
        }
    }

    /* If we just finished a PRE-Y, stop PED now */
    if (st->is_prep_y) {
        ped_stop_if_prep_finished();
    }

    /* If we were forced into a yellow, on yellow completion jump to ALL-RED */
    if (train_request && train_preempt_to_allred) {
        if (*cur == N_R3_RS_Y || *cur == N_R3_L_Y) { *cur = N_ALL_RED_2; return; }
        if (*cur == N_R2_RS_Y || *cur == N_R2_L_Y) { *cur = N_ALL_RED_1; return; }
    }

    /* At SAFE ALL-RED: allow entry to TRAIN */
    if (st->is_safe_allred) {
        train_preempt_to_allred = 0;

        if (train_request) {
            train_request = 0;
            train_active  = 1;
            train_clear_pending = 0;

            in_train_state = 1;
            tr = TR_S01_PREP_R3;  /* TRAIN S01 = PRE-Y (as requested) */
            train_ui_step = 1;
            notify_train_begin();
            return;
        }
    }

    *cur = st->next;
}

static void train_run_until_exit(normal_state_t *normal_state_after)
{
    static int clear_notified_once = 0;
    if (train_clear_pending && !clear_notified_once) {
        notify_train_clear();
        clear_notified_once = 1;
    }
    if (!train_clear_pending) clear_notified_once = 0;

    train_step();

    /* Exit at safe all-red */
    if (train_clear_pending && train_is_safe_allred(tr)) {
        train_active = 0;
        train_clear_pending = 0;
        in_train_state = 0;

        notify_train_over();

        /* return NORMAL at ALL-RED so UI keeps S01=ALL-RED */
        *normal_state_after = N_ALL_RED_1;
        normal_ui_set_start(N_ALL_RED_1);
    }
}

/* ================= QNET SETUP ================= */
static void qnet_setup_server(void)
{
    g_attach = name_attach(NULL, ATTACH_POINT, 0);
    if (!g_attach) {
        perror("name_attach");
        exit(EXIT_FAILURE);
    }

    printf("[vm8_local2] attached at /dev/name/local/%s\n", ATTACH_POINT);
    printf("[vm8_local2] VM7 connect to /net/<vm8_node>/dev/name/local/%s\n\n", ATTACH_POINT);
    fflush(stdout);
}

/* ================= MAIN ================= */
int main(void)
{
    printf("Local Control 2 (VM8, QNET INPUT) - Local1 style\n");
    printf("Attach point: %s\n", ATTACH_POINT);
    printf("Events from VM7: t=train, c=clear, p=ped\n\n");
    fflush(stdout);

    qnet_setup_server();

    /* NORMAL S01 must be ALL-RED */
    normal_state_t ns = N_ALL_RED_1;
    normal_ui_set_start(N_ALL_RED_1);

    while (1) {
        if (!in_train_state) normal_step(&ns);
        else                 train_run_until_exit(&ns);
    }
    return 0;
}
