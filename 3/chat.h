#ifndef CHAT_H
#define CHAT_H

#include <mqueue.h>
#include <signal.h>

#define CHAT_MESSAGE_SIZE 256
#define CHAT_MESSAGE_PRIORITY 0
#define CHAT_STOP_PRIORITY 1

typedef struct
{
    mqd_t send_queue;
    mqd_t receive_queue;
    int is_creator;
    char queue1_name[256];
    char queue2_name[256];
} Chat;

int chat_open(Chat *chat, const char *base_name);
void chat_run(Chat *chat, volatile sig_atomic_t *stop_requested);
void chat_close(Chat *chat);

#endif