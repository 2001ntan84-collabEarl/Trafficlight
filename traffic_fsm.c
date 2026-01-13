/*
 * Real-Time Systems - Lab 5 (QNX)
 * Traffic Light FSM (NORMAL + TRAIN + PEDESTRIAN) using POSIX mqueue
 *
 * Keyboard events (send to /traffic_mq):
 *   t = Train detected  (request train mode)
 *   c = Train cleared   (request exit train mode at next SAFE all-red)
 *   p = Ped button      (ped_request=1)
 *
 * Notify messages:
 *   *** TRAIN BEGIN ***
 *   *** TRAIN OVER  ***
 *   >>> TRAIN PREEMPT: forcing YELLOW immediately <<<
 *   >>> TRAIN CLEAR: will exit at next SAFE ALL-RED <<<
 *   *** PED BEGIN   ***
 *   *** PED OVER    ***
 *
 * Behaviour:
 * 1) Press 't' during NORMAL GREEN:
 *    - Force matching YELLOW immediately (within ~100ms)
 *    - After forced YELLOW finishes, jump DIRECTLY to ALL-RED (skip left phases)
 *    - Then enter TRAIN mode
 *
 * 2) Press 'c' during TRAIN (Option A):
 *    - DO NOT change lights immediately
 *    - Set train_clear_pending=1
 *    - Keep running the TRAIN sequence until the next SAFE ALL-RED checkpoint
 *      (T_ALL_RED_A, T_ALL_RED_B, or T_DECISION_ALL_RED_4)
 *    - Exit to NORMAL from that checkpoint
 *
 * CHANGE REQUEST:
 * - TRAIN "state 0" removed.
 * - TRAIN starts directly at TRAIN state 1 (T_R3_NS_SRL_G_1).
 * - Everything else remains the same.
 */

#include <stdio.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>

/* ================= MQ CONFIG ================= */
#define QUEUE_NAME "/traffic_mq"
#define MSG_SIZE   2

#define EVT_TRAIN_DETECT  't'
#define EVT_TRAIN_CLEAR   'c'
#define EVT_PED_PRESS     'p'

/* ================= TIMINGS (seconds) ================= */
#define T_RS_GREEN   20
#define T_L_GREEN    12
#define T_YELLOW      4
#define T_ALL_RED     2

#define T_TR_1_G      8
#define T_TR_2_G      8
#define T_TR_3_G     15
#define T_TR_Y        4
#define T_TR_R        2

#define T_PED_WALK    8
#define T_PED_FLASH   4
#define T_PED_CLR     2

/* ================= FLAGS (set by events) ================= */
static int train_request = 0;        /* request to enter TRAIN */
static int train_active  = 0;        /* TRAIN is active */
static int train_clear_pending = 0;  /* Option A: exit TRAIN at next safe ALL-RED */
static int ped_request   = 0;

/* notifications state */
static int in_train_mode = 0;
static int in_ped_mode   = 0;

/* notify-once flag for forced yellow */
static int train_preempt_notified = 0;

/* after forced yellow, go directly to ALL-RED (skip left phases) */
static int train_preempt_to_allred = 0;

/* ================= STATE ENUM =================
 * Explicit numbering keeps mode_index logic stable:
 *  NORMAL: 0..9
 *  TRAIN : 10..18   (starts at TRAIN S1)
 *  PED   : 20..22
 */
typedef enum {
    /* ---------- NORMAL (S0..S9) ---------- */
    N_R3_RS_G = 0,
    N_R3_RS_Y,
    N_R3_L_G,
    N_R3_L_Y,
    N_ALL_RED_1,
    N_R1_RS_G,
    N_R1_RS_Y,
    N_R1_L_G,
    N_R1_L_Y,
    N_ALL_RED_2,

    /* ---------- TRAIN (S1..S9 mapped from 10..) ---------- */
    T_R3_NS_SRL_G_1 = 10,
    T_R3_NS_SRL_Y_1,
    T_ALL_RED_A,          /* SAFE */
    T_R3_SN_LR_G_2,
    T_R3_SN_LR_Y_2,
    T_ALL_RED_B,          /* SAFE */
    T_R1_RESTRICT_G_3,
    T_R1_RESTRICT_Y_3,
    T_DECISION_ALL_RED_4, /* SAFE (decision/exit) */

    /* ---------- PEDESTRIAN ---------- */
    P_WALK = 20,
    P_FLASH,
    P_CLEAR_ALL_RED
} state_t;

/* After pedestrian completes, return here */
static state_t ped_return_state = N_R3_RS_G;

/* ================= MQ ================= */
static mqd_t mq = (mqd_t)-1;

static void mq_setup_server(void)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg  = 20;
    attr.mq_msgsize = MSG_SIZE;

    mq_unlink(QUEUE_NAME);

    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open (traffic server)");
    } else {
        printf("Traffic FSM created queue %s\n", QUEUE_NAME);
        fflush(stdout);
    }
}

static void poll_events_from_mq(void)
{
    if (mq == (mqd_t)-1) return;

    char buf[MSG_SIZE];

    while (1) {
        ssize_t n = mq_receive(mq, buf, MSG_SIZE, NULL);
        if (n == -1) {
            if (errno == EAGAIN) break;
            perror("mq_receive");
            break;
        }

        char ev = buf[0];
        if (ev == EVT_TRAIN_DETECT) {
            train_request = 1;
            train_active  = 1;
            /* if train is requested again, cancel any pending clear */
            train_clear_pending = 0;
            train_preempt_notified = 0;
        } else if (ev == EVT_TRAIN_CLEAR) {
            /* Option A: request exit at next safe all-red */
            train_clear_pending = 1;
            train_request = 0; /* cancel any pending enter */
        } else if (ev == EVT_PED_PRESS) {
            ped_request = 1;
        }
    }
}

/* ================= NOTIFY HELPERS ================= */
static void notify_train_begin(void)   { printf("\n*** TRAIN BEGIN ***\n\n"); fflush(stdout); }
static void notify_train_over(void)    { printf("\n*** TRAIN OVER  ***\n\n"); fflush(stdout); }
static void notify_train_preempt(void) { printf("\n>>> TRAIN PREEMPT: forcing YELLOW immediately <<<\n\n"); fflush(stdout); }
static void notify_train_clear(void)   { printf("\n>>> TRAIN CLEAR: will exit at next SAFE ALL-RED <<<\n\n"); fflush(stdout); }
static void notify_ped_begin(void)     { printf("\n*** PED BEGIN   ***\n\n"); fflush(stdout); }
static void notify_ped_over(void)      { printf("\n*** PED OVER    ***\n\n"); fflush(stdout); }

/* ================= MODE/INDEX ================= */
static const char* mode_of(state_t s)
{
    if (s <= N_ALL_RED_2) return "NORMAL";
    if (s >= T_R3_NS_SRL_G_1 && s <= T_DECISION_ALL_RED_4) return "TRAIN ";
    return "PED   ";
}

static int mode_index_of(state_t s)
{
    if (s <= N_ALL_RED_2) return (int)s;          /* 0..9 */
    if (s <= T_DECISION_ALL_RED_4) return (int)s - 10; /* 0..8 (TRAIN S1 prints as S0 here? see note below) */
    return (int)s - 20;                           /* 0.. */
}

/* NOTE:
 * With TRAIN starting at enum value 10, mode_index_of prints TRAIN as S0..S8.
 * If you want it to print S1..S9 instead, change return to: (int)s - 9.
 * You did NOT ask for print renumbering, only to start at TRAIN state 1.
 */

static unsigned duration_of(state_t s)
{
    switch (s) {
        /* NORMAL */
        case N_R3_RS_G:  return T_RS_GREEN;
        case N_R3_RS_Y:  return T_YELLOW;
        case N_R3_L_G:   return T_L_GREEN;
        case N_R3_L_Y:   return T_YELLOW;
        case N_ALL_RED_1:return T_ALL_RED;

        case N_R1_RS_G:  return T_RS_GREEN;
        case N_R1_RS_Y:  return T_YELLOW;
        case N_R1_L_G:   return T_L_GREEN;
        case N_R1_L_Y:   return T_YELLOW;
        case N_ALL_RED_2:return T_ALL_RED;

        /* TRAIN (starts at S1) */
        case T_R3_NS_SRL_G_1:      return T_TR_1_G;
        case T_R3_NS_SRL_Y_1:      return T_TR_Y;
        case T_ALL_RED_A:          return T_TR_R;
        case T_R3_SN_LR_G_2:       return T_TR_2_G;
        case T_R3_SN_LR_Y_2:       return T_TR_Y;
        case T_ALL_RED_B:          return T_TR_R;
        case T_R1_RESTRICT_G_3:    return T_TR_3_G;
        case T_R1_RESTRICT_Y_3:    return T_TR_Y;
        case T_DECISION_ALL_RED_4: return T_TR_R;

        /* PED */
        case P_WALK:          return T_PED_WALK;
        case P_FLASH:         return T_PED_FLASH;
        case P_CLEAR_ALL_RED: return T_PED_CLR;

        default: return 0;
    }
}

static int is_train_state(state_t s)
{
    return (s >= T_R3_NS_SRL_G_1 && s <= T_DECISION_ALL_RED_4);
}

static int is_normal_state(state_t s)
{
    return (s >= N_R3_RS_G && s <= N_ALL_RED_2);
}

static int is_normal_green(state_t s)
{
    return (s == N_R3_RS_G || s == N_R3_L_G ||
            s == N_R1_RS_G || s == N_R1_L_G);
}

static state_t normal_green_to_yellow(state_t s)
{
    switch (s) {
        case N_R3_RS_G: return N_R3_RS_Y;
        case N_R3_L_G:  return N_R3_L_Y;
        case N_R1_RS_G: return N_R1_RS_Y;
        case N_R1_L_G:  return N_R1_L_Y;
        default:        return s;
    }
}

/* ================= PED SAFE CHECK ================= */
static int ped_safe_checkpoint(state_t s)
{
    return (s == N_ALL_RED_1 || s == N_ALL_RED_2 ||
            s == T_ALL_RED_A || s == T_ALL_RED_B || s == T_DECISION_ALL_RED_4);
}

static int try_start_ped_if_safe(state_t *cur)
{
    if (!cur) return 0;
    if (!ped_request) return 0;
    if (!ped_safe_checkpoint(*cur)) return 0;

    if (train_active && !train_clear_pending) {
        /* train running: only allow ped at train safe checkpoints */
        if (!(*cur == T_ALL_RED_A || *cur == T_ALL_RED_B || *cur == T_DECISION_ALL_RED_4)) return 0;
        ped_return_state = T_R3_SN_LR_G_2;
    } else {
        /* normal mode (or train is clearing): return to next normal phase */
        if (*cur == N_ALL_RED_1) ped_return_state = N_R1_RS_G;
        else if (*cur == N_ALL_RED_2) ped_return_state = N_R3_RS_G;
        else ped_return_state = N_R3_RS_G;
    }

    ped_request = 0;
    *cur = P_WALK;

    if (!in_ped_mode) {
        in_ped_mode = 1;
        notify_ped_begin();
    }
    return 1;
}

/* ================= OUTPUT ================= */
static void print_state_outputs(state_t s)
{
    const char *r3_sn = "RED";
    const char *r3_ns = "RED";
    const char *r1_we = "RED";
    const char *r1_ew = "RED";
    const char *ped   = "RED";

    switch (s) {
        /* NORMAL */
        case N_R3_RS_G: r3_sn = "RS-G"; r3_ns = "RS-G"; break;
        case N_R3_RS_Y: r3_sn = "RS-Y"; r3_ns = "RS-Y"; break;
        case N_R3_L_G:  r3_sn = "L-G";  r3_ns = "L-G";  break;
        case N_R3_L_Y:  r3_sn = "L-Y";  r3_ns = "L-Y";  break;

        case N_R1_RS_G: r1_we = "RS-G"; r1_ew = "RS-G"; break;
        case N_R1_RS_Y: r1_we = "RS-Y"; r1_ew = "RS-Y"; break;
        case N_R1_L_G:  r1_we = "L-G";  r1_ew = "L-G";  break;
        case N_R1_L_Y:  r1_we = "L-Y";  r1_ew = "L-Y";  break;

        /* TRAIN */
        case T_R3_NS_SRL_G_1: r3_ns = "SRL-G"; break;
        case T_R3_NS_SRL_Y_1: r3_ns = "SRL-Y"; break;
        case T_R3_SN_LR_G_2:  r3_sn = "LR-G";  break;
        case T_R3_SN_LR_Y_2:  r3_sn = "LR-Y";  break;

        case T_R1_RESTRICT_G_3:
            r1_we = "SR-G";
            r1_ew = "SL-G";
            break;
        case T_R1_RESTRICT_Y_3:
            r1_we = "SR-Y";
            r1_ew = "SL-Y";
            break;

        /* PED */
        case P_WALK:  ped = "WALK";  break;
        case P_FLASH: ped = "FLASH"; break;

        default:
            break;
    }

    printf("[%s S%d] (%us) | R3(S->N)=%-6s | R3(N->S)=%-6s | R1(W->E)=%-6s | R1(E->W)=%-6s | PED=%-5s\n",
           mode_of(s), mode_index_of(s), duration_of(s),
           r3_sn, r3_ns, r1_we, r1_ew, ped);
    fflush(stdout);
}

/* ================= INTERRUPTIBLE WAIT =================
 * - If 't' during NORMAL GREEN: force YELLOW now, set train_preempt_to_allred (skip left)
 * - If 'c' during TRAIN: do NOT change lights; just sets train_clear_pending (handled at safe checkpoints)
 */
static void wait_seconds_interruptible(unsigned total_sec, state_t *cur)
{
    if (total_sec == 0) return;

    const long step_ns = 100L * 1000L * 1000L; /* 100ms */
    unsigned steps  = (unsigned)((total_sec * 1000U) / 100U);
    unsigned rem_ms = (total_sec * 1000U) % 100U;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = step_ns;

    for (unsigned i = 0; i < steps; i++) {
        nanosleep(&ts, NULL);
        poll_events_from_mq();

        /* PREEMPT: normal green -> yellow immediately */
        if (cur && train_request && !in_train_mode && is_normal_green(*cur)) {
            if (!train_preempt_notified) {
                notify_train_preempt();
                train_preempt_notified = 1;
            }
            train_preempt_to_allred = 1;
            *cur = normal_green_to_yellow(*cur);
            return;
        }
    }

    if (rem_ms > 0) {
        struct timespec ts2;
        ts2.tv_sec = 0;
        ts2.tv_nsec = (long)rem_ms * 1000L * 1000L;
        nanosleep(&ts2, NULL);
        poll_events_from_mq();

        if (cur && train_request && !in_train_mode && is_normal_green(*cur)) {
            if (!train_preempt_notified) {
                notify_train_preempt();
                train_preempt_notified = 1;
            }
            train_preempt_to_allred = 1;
            *cur = normal_green_to_yellow(*cur);
            return;
        }
    }
}

/* ================= TRAIN EXIT CHECK (Option A) ================= */
static int should_exit_train_now(state_t s)
{
    if (!train_clear_pending) return 0;
    return (s == T_ALL_RED_A || s == T_ALL_RED_B || s == T_DECISION_ALL_RED_4);
}

static void do_exit_train_to_normal(state_t *cur)
{
    train_active = 0;
    train_clear_pending = 0;
    train_request = 0;

    /* exit to normal start */
    *cur = N_R3_RS_G;

    if (in_train_mode) {
        in_train_mode = 0;
        notify_train_over();
    }
}

/* ================= FSM STEP ================= */
static void SingleStep_SM(state_t *cur)
{
    if (!cur) return;

    print_state_outputs(*cur);
    poll_events_from_mq();

    /* notify once when clear is requested */
    static int clear_notified_once = 0;
    if (train_clear_pending && !clear_notified_once) {
        notify_train_clear();
        clear_notified_once = 1;
    }
    if (!train_clear_pending) clear_notified_once = 0;

    switch (*cur) {

        /* ===================== NORMAL ===================== */
        case N_R3_RS_G:
            wait_seconds_interruptible(T_RS_GREEN, cur);
            if (*cur != N_R3_RS_G) break;
            *cur = N_R3_RS_Y;
            break;

        case N_R3_RS_Y:
            wait_seconds_interruptible(T_YELLOW, cur);
            if (*cur != N_R3_RS_Y) break;

            if (train_request && train_preempt_to_allred) {
                *cur = N_ALL_RED_1; /* skip left phases */
            } else {
                *cur = N_R3_L_G;
            }
            break;

        case N_R3_L_G:
            wait_seconds_interruptible(T_L_GREEN, cur);
            if (*cur != N_R3_L_G) break;
            *cur = N_R3_L_Y;
            break;

        case N_R3_L_Y:
            wait_seconds_interruptible(T_YELLOW, cur);
            if (*cur != N_R3_L_Y) break;
            *cur = N_ALL_RED_1;
            break;

        case N_ALL_RED_1:
            train_preempt_to_allred = 0;
            wait_seconds_interruptible(T_ALL_RED, cur);
            poll_events_from_mq();

            if (train_request) {
                train_request = 0;
                train_active  = 1;
                train_clear_pending = 0;

                /* TRAIN now starts at S1 */
                *cur = T_R3_NS_SRL_G_1;

                if (!in_train_mode) { in_train_mode = 1; notify_train_begin(); }
                break;
            }

            if (try_start_ped_if_safe(cur)) break;
            *cur = N_R1_RS_G;
            break;

        case N_R1_RS_G:
            wait_seconds_interruptible(T_RS_GREEN, cur);
            if (*cur != N_R1_RS_G) break;
            *cur = N_R1_RS_Y;
            break;

        case N_R1_RS_Y:
            wait_seconds_interruptible(T_YELLOW, cur);
            if (*cur != N_R1_RS_Y) break;

            if (train_request && train_preempt_to_allred) {
                *cur = N_ALL_RED_2; /* skip left phases */
            } else {
                *cur = N_R1_L_G;
            }
            break;

        case N_R1_L_G:
            wait_seconds_interruptible(T_L_GREEN, cur);
            if (*cur != N_R1_L_G) break;
            *cur = N_R1_L_Y;
            break;

        case N_R1_L_Y:
            wait_seconds_interruptible(T_YELLOW, cur);
            if (*cur != N_R1_L_Y) break;
            *cur = N_ALL_RED_2;
            break;

        case N_ALL_RED_2:
            train_preempt_to_allred = 0;
            wait_seconds_interruptible(T_ALL_RED, cur);
            poll_events_from_mq();

            if (train_request) {
                train_request = 0;
                train_active  = 1;
                train_clear_pending = 0;

                /* TRAIN now starts at S1 */
                *cur = T_R3_NS_SRL_G_1;

                if (!in_train_mode) { in_train_mode = 1; notify_train_begin(); }
                break;
            }

            if (try_start_ped_if_safe(cur)) break;
            *cur = N_R3_RS_G;
            break;

        /* ===================== TRAIN ===================== */
        case T_R3_NS_SRL_G_1:
            wait_seconds_interruptible(T_TR_1_G, cur);
            *cur = T_R3_NS_SRL_Y_1;
            break;

        case T_R3_NS_SRL_Y_1:
            wait_seconds_interruptible(T_TR_Y, cur);
            *cur = T_ALL_RED_A;
            break;

        case T_ALL_RED_A:
            wait_seconds_interruptible(T_TR_R, cur);
            poll_events_from_mq();

            if (should_exit_train_now(*cur)) { do_exit_train_to_normal(cur); break; }
            if (try_start_ped_if_safe(cur)) break;

            *cur = T_R3_SN_LR_G_2;
            break;

        case T_R3_SN_LR_G_2:
            wait_seconds_interruptible(T_TR_2_G, cur);
            *cur = T_R3_SN_LR_Y_2;
            break;

        case T_R3_SN_LR_Y_2:
            wait_seconds_interruptible(T_TR_Y, cur);
            *cur = T_ALL_RED_B;
            break;

        case T_ALL_RED_B:
            wait_seconds_interruptible(T_TR_R, cur);
            poll_events_from_mq();

            if (should_exit_train_now(*cur)) { do_exit_train_to_normal(cur); break; }
            if (try_start_ped_if_safe(cur)) break;

            *cur = T_R1_RESTRICT_G_3;
            break;

        case T_R1_RESTRICT_G_3:
            wait_seconds_interruptible(T_TR_3_G, cur);
            *cur = T_R1_RESTRICT_Y_3;
            break;

        case T_R1_RESTRICT_Y_3:
            wait_seconds_interruptible(T_TR_Y, cur);
            *cur = T_DECISION_ALL_RED_4;
            break;

        case T_DECISION_ALL_RED_4:
            wait_seconds_interruptible(T_TR_R, cur);
            poll_events_from_mq();

            if (should_exit_train_now(*cur)) { do_exit_train_to_normal(cur); break; }
            if (try_start_ped_if_safe(cur)) break;

            /* keep train looping if not clearing */
            if (train_active && !train_clear_pending) {
                *cur = T_R3_SN_LR_G_2;
            } else {
                /* default safe exit */
                do_exit_train_to_normal(cur);
            }
            break;

        /* ===================== PEDESTRIAN ===================== */
        case P_WALK:
            wait_seconds_interruptible(T_PED_WALK, cur);
            *cur = P_FLASH;
            break;

        case P_FLASH:
            wait_seconds_interruptible(T_PED_FLASH, cur);
            *cur = P_CLEAR_ALL_RED;
            break;

        case P_CLEAR_ALL_RED:
            wait_seconds_interruptible(T_PED_CLR, cur);
            if (in_ped_mode) { in_ped_mode = 0; notify_ped_over(); }
            *cur = ped_return_state;
            break;

        default:
            *cur = N_R3_RS_G;
            break;
    }

    /* ensure begin notify if train states entered */
    if (is_train_state(*cur) && !in_train_mode) {
        in_train_mode = 1;
        notify_train_begin();
    }

    /* clean train flags if we are back in normal */
    if (is_normal_state(*cur) && in_train_mode) {
        in_train_mode = 0;
        notify_train_over();
    }
}

int main(void)
{
    printf("local control 1\n");
    printf("Queue: %s\n", QUEUE_NAME);
    printf("Keyboard events: t=train detect, c=train clear, p=ped press\n\n");
    fflush(stdout);

    mq_setup_server();

    state_t s = N_R3_RS_G;
    while (1) {
        SingleStep_SM(&s);
    }
    return 0;
}
