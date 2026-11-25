#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

#include "message.h"

typedef enum
{
    PROTOCOL_SUBSCRIBE,
    PROTOCOL_UNSUBSCRIBE,
    PROTOCOL_PUBLISH
} ProtocolCommand;

typedef struct
{
    ProtocolCommand command;
    pid_t pid;
    char topic[TOPIC_SIZE];
    char payload[MESSAGE_TEXT_SIZE];
} ProtocolMessage;

void protocol_make_subscribe(Message *message, pid_t pid, const char *topic);
void protocol_make_unsubscribe(Message *message, pid_t pid, const char *topic);
void protocol_make_publish(Message *message, pid_t pid, const char *topic, const char *payload);
void protocol_parse(const Message *message, ProtocolMessage *parsed);
void protocol_make_delivery(Message *message, pid_t recipient, const char *topic,
                            const char *payload);

#endif