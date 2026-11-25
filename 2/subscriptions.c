#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "subscriptions.h"

int subscriptions_add(Subscription **head, pid_t pid, const char *topic)
{
    for (Subscription *item = *head; item != NULL; item = item->next)
    {
        if (item->pid == pid && strcmp(item->topic, topic) == 0)
        {
            return 0;
        }
    }

    Subscription *item = malloc(sizeof(*item));
    if (item == NULL)
    {
        return -1;
    }

    item->pid = pid;
    snprintf(item->topic, sizeof(item->topic), "%s", topic);
    item->next = *head;
    *head = item;
    return 0;
}

void subscriptions_remove(Subscription **head, pid_t pid, const char *topic)
{
    Subscription **link = head;
    while (*link != NULL)
    {
        Subscription *item = *link;
        if (item->pid == pid && strcmp(item->topic, topic) == 0)
        {
            *link = item->next;
            free(item);
            return;
        }
        link = &item->next;
    }
}

void subscriptions_free(Subscription **head)
{
    while (*head != NULL)
    {
        Subscription *item = *head;
        *head = item->next;
        free(item);
    }
}