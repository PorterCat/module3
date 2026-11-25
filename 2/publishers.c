#include <stdlib.h>

#include "publishers.h"

int publishers_add(Publisher **head, pid_t pid)
{
    for (Publisher *current = *head; current != NULL; current = current->next)
    {
        if (current->pid == pid)
        {
            return 0;
        }
    }

    Publisher *node = malloc(sizeof(*node));

    if (node == NULL)
    {
        return -1;
    }

    node->pid = pid;
    node->next = *head;
    *head = node;

    return 0;
}

void publishers_remove(Publisher **head, pid_t pid)
{
    Publisher **current = head;

    while (*current != NULL)
    {
        Publisher *node = *current;

        if (node->pid == pid)
        {
            *current = node->next;
            free(node);
            return;
        }

        current = &node->next;
    }
}

void publishers_free(Publisher **head)
{
    while (*head != NULL)
    {
        Publisher *node = *head;
        *head = node->next;
        free(node);
    }
}