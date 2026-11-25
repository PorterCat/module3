#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

#include "queue.h"

#define QUEUE_KEY 0x1111

int queue_create()
{
    return msgget(QUEUE_KEY, IPC_CREAT | IPC_EXCL | 0600);
}

int queue_open()
{
    return msgget(QUEUE_KEY, 0600);
}

int queue_remove(int queue_id)
{
    return msgctl(queue_id, IPC_RMID, NULL);
}

int queue_send(int queue_id, const Message *message)
{
    return msgsnd(queue_id, message, strlen(message->mtext) + 1, 0);
}

ssize_t queue_receive(int queue_id, Message *message, long type)
{
    ssize_t size = msgrcv(queue_id, message, sizeof(message->mtext), type, 0);

    if (size >= 0 && (size_t)size < sizeof(message->mtext))
    {
        message->mtext[size] = '\0';
    }

    return size;
}