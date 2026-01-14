
/*
 * Keyboard Event Process (QNX) - Sender
 * Sends events to the traffic FSM via POSIX mqueue (/traffic_mq).
 *
 * Keys:
 *   t = Train detected
 *   c = Train cleared
 *   p = Ped button pressed
 *   q = Quit
 *
 * Improvements vs your version:
 * - Uses getchar() (no scanf blocking/whitespace issues)
 * - Flushes extra chars until newline
 * - Cleaner retry logic + clearer errors
 * - Ignores unknown keys silently (or with message)
 */

#include <stdio.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define QUEUE_NAME "/traffic_mq"
#define MSG_SIZE   2

#define EVT_TRAIN_DETECT  't'
#define EVT_TRAIN_CLEAR   'c'
#define EVT_PED_PRESS     'p'

static mqd_t open_queue_writer_blocking(void)
{
    mqd_t mq;

    /* Wait until traffic FSM creates the queue */
    for (;;) {
        mq = mq_open(QUEUE_NAME, O_WRONLY);
        if (mq != (mqd_t)-1) return mq;

        /* Expected until server starts */
        fprintf(stderr, "[keyboard] waiting for queue %s: %s\n",
                QUEUE_NAME, strerror(errno));
        sleep(1);
    }
}

static void flush_line(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) { /* discard */ }
}

static int send_event(mqd_t mq, char ev)
{
    char msg[MSG_SIZE];
    msg[0] = ev;
    msg[1] = '\0';

    if (mq_send(mq, msg, MSG_SIZE, 0) == -1) {
        fprintf(stderr, "[keyboard] mq_send failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int main(void)
{
    mqd_t mq = open_queue_writer_blocking();

    printf("[keyboard] connected to %s\n", QUEUE_NAME);
    printf("Commands:\n");
    printf("  t = Train detected\n");
    printf("  c = Train cleared\n");
    printf("  p = Ped button pressed\n");
    printf("  q = Quit\n\n");
    fflush(stdout);

    for (;;) {
        int ch = getchar();
        if (ch == EOF) break;

        /* ignore whitespace */
        if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') continue;

        /* if user typed more chars, drop rest of the line */
        flush_line();

        char ev = 0;
        if (ch == 't' || ch == 'T') ev = EVT_TRAIN_DETECT;
        else if (ch == 'c' || ch == 'C') ev = EVT_TRAIN_CLEAR;
        else if (ch == 'p' || ch == 'P') ev = EVT_PED_PRESS;
        else if (ch == 'q' || ch == 'Q') break;
        else {
            printf("[keyboard] ignored '%c' (use t/c/p/q)\n", ch);
            fflush(stdout);
            continue;
        }

        if (send_event(mq, ev) == 0) {
            printf("[keyboard] sent '%c'\n", ev);
            fflush(stdout);
        }
    }

    mq_close(mq);
    printf("[keyboard] exit\n");
    fflush(stdout);
    return 0;
}

