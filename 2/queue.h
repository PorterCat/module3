#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>

#include "message.h"

int queue_create();
int queue_open();
int queue_remove(int queue_id);

int queue_send(int queue_id, const Message *message);
ssize_t queue_receive(int queue_id, Message *message, long type);

#endif