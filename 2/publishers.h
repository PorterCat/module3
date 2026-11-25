#ifndef PUBLISHERS_H
#define PUBLISHERS_H

#include <sys/types.h>

typedef struct Publisher
{
    pid_t pid;
    struct Publisher *next;
} Publisher;

int publishers_add(Publisher **head, pid_t pid);
void publishers_remove(Publisher **head, pid_t pid);
void publishers_free(Publisher **head);

#endif