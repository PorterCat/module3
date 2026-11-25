#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#include <sys/types.h>

#include "message.h"

typedef struct Subscription
{
    pid_t pid;
    char topic[TOPIC_SIZE];
    struct Subscription *next;
} Subscription;

int subscriptions_add(Subscription **head, pid_t pid, const char *topic);
void subscriptions_remove(Subscription **head, pid_t pid, const char *topic);
void subscriptions_free(Subscription **head);

#endif