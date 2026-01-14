/*
 * VM7: Keyboard Client (broadcast to VM6 + VM8)
 *
 * PURPOSE
 * - Read keys locally on VM7
 * - Send events ('t','c','p') to BOTH servers (VM6 and VM8) via QNET
 *
 * REQUIREMENTS
 * - VM6 server: name_attach(NULL, "traffic_evt", 0)
 * - VM8 server: name_attach(NULL, "traffic_evt", 0)
 * - VM7 can see both nodes in: ls /net   (must show vm6 and vm8)
 *
 * NOTES
 * - This client MUST match evt_msg_t / evt_reply_t layout expected by server.
 * - If a server is not running, that path will fail name_open and be skipped.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/dispatch.h>   // name_open(), MsgSend(), name_close()

/* ---- MUST match each server's name_attach point ---- */
#define AP_NAME "traffic_evt"

/* ---- server paths on each node ---- */
#define VM6_PATH "/net/vm6/dev/name/local/" AP_NAME
#define VM8_PATH "/net/vm8/dev/name/local/" AP_NAME

/* keys/events */
#define EVT_TRAIN_DETECT  't'
#define EVT_TRAIN_CLEAR   'c'
#define EVT_PED_PRESS     'p'

/* EXACT message structs expected by server */
typedef struct {
    _Uint16t type;
    _Uint16t subtype;
    char     ev;
    char     pad[3];
    int      client_id;
} evt_msg_t;

typedef struct {
    _Uint16t type;
    _Uint16t subtype;
    char     text[64];
} evt_reply_t;

/* flush extra chars until newline so user can type "t + enter" safely */
static void flush_line(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) { /* discard */ }
}

static int try_open(const char *path)
{
    int coid = name_open(path, 0);
    if (coid == -1) {
        printf("[kb_vm7] name_open failed for %s: %s\n", path, strerror(errno));
        return -1;
    }
    printf("[kb_vm7] connected to %s (coid=%d)\n", path, coid);
    return coid;
}

static void try_send(int coid, const char *tag, char ev, int client_id)
{
    evt_msg_t msg;
    evt_reply_t rep;
    memset(&msg, 0, sizeof(msg));
    memset(&rep, 0, sizeof(rep));

    msg.type = 0x22;
    msg.subtype = 0;
    msg.ev = ev;
    msg.client_id = client_id;

    if (MsgSend(coid, &msg, sizeof(msg), &rep, sizeof(rep)) == -1) {
        printf("[kb_vm7] %s MsgSend failed: %s\n", tag, strerror(errno));
        return;
    }

    printf("[kb_vm7] %s sent '%c' -> reply: %s\n", tag, ev, rep.text);
}

int main(void)
{
    printf("[kb_vm7] Keyboard Client (broadcast)\n");
    printf("[kb_vm7] VM6 path: %s\n", VM6_PATH);
    printf("[kb_vm7] VM8 path: %s\n\n", VM8_PATH);

    /* connect to both (if available) */
    int coid6 = try_open(VM6_PATH);
    int coid8 = try_open(VM8_PATH);

    if (coid6 == -1 && coid8 == -1) {
        printf("[kb_vm7] No servers connected. Start VM6/VM8 servers first.\n");
        return EXIT_FAILURE;
    }

    printf("\nCommands: t=train, c=clear, p=ped, q=quit\n\n");
    fflush(stdout);

    for (;;) {
        int ch = getchar();
        if (ch == EOF) break;

        if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') continue;
        flush_line();

        char ev = 0;
        if      (ch == 't' || ch == 'T') ev = EVT_TRAIN_DETECT;
        else if (ch == 'c' || ch == 'C') ev = EVT_TRAIN_CLEAR;
        else if (ch == 'p' || ch == 'P') ev = EVT_PED_PRESS;
        else if (ch == 'q' || ch == 'Q') break;
        else {
            printf("[kb_vm7] ignored '%c' (use t/c/p/q)\n", ch);
            fflush(stdout);
            continue;
        }

        /* If a server wasn’t connected at startup, try reconnect on each keypress */
        if (coid6 == -1) coid6 = try_open(VM6_PATH);
        if (coid8 == -1) coid8 = try_open(VM8_PATH);

        /* send to whichever is connected */
        if (coid6 != -1) try_send(coid6, "VM6", ev, 700);
        if (coid8 != -1) try_send(coid8, "VM8", ev, 800);

        fflush(stdout);
    }

    if (coid6 != -1) name_close(coid6);
    if (coid8 != -1) name_close(coid8);

    printf("[kb_vm7] exit\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}
