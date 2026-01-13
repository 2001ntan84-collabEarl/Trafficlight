/*
 * Keyboard Event Process (QNX) - Sender
 * Sends events to the traffic FSM via POSIX mqueue.
 *
 * Keys:
 *   t = Train detected   (sets train_request=1 and train_active=1)
 *   c = Train cleared    (sets train_active=0)
 *   p = Ped button press (sets ped_request=1)
 */

#include <stdio.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>

#define QUEUE_NAME "/traffic_mq"
#define MSG_SIZE   2

#define EVT_TRAIN_DETECT  't'
#define EVT_TRAIN_CLEAR   'c'
#define EVT_PED_PRESS     'p'

int main(void)
{
    mqd_t mq;
    char msg[MSG_SIZE];
    char c;

    /* Wait until traffic FSM creates the queue */
    while ((mq = mq_open(QUEUE_NAME, O_WRONLY)) == (mqd_t)-1) {
        perror("keyboard waiting for queue");
        sleep(1);
    }

    printf("Keyboard Event Process connected to %s\n", QUEUE_NAME);
    printf("Commands:\n");
    printf("  t = Train detected\n");
    printf("  c = Train cleared\n");
    printf("  p = Ped button pressed\n\n");

    while (1) {
        scanf(" %c", &c);

        if (c == 't' || c == 'T') c = EVT_TRAIN_DETECT;
        else if (c == 'c' || c == 'C') c = EVT_TRAIN_CLEAR;
        else if (c == 'p' || c == 'P') c = EVT_PED_PRESS;
        else {
            printf("Ignored. Use t/c/p.\n");
            continue;
        }

        msg[0] = c;
        msg[1] = '\0';

        if (mq_send(mq, msg, MSG_SIZE, 0) == -1) {
            perror("mq_send");
        }
    }

    return 0;
}
