#include "protocol.h"

#include <stdio.h>
#include <string.h>

void protocol_make_subscribe(Message *message, pid_t pid, const char *topic)
{
    message->mtype = BROKER_MTYPE;
    sprintf(message->mtext, "subscribe,%ld,%s", (long)pid, topic);
}

void protocol_make_unsubscribe(Message *message, pid_t pid, const char *topic)
{
    message->mtype = BROKER_MTYPE;
    sprintf(message->mtext, "unsubscribe,%ld,%s", (long)pid, topic);
}

void protocol_make_publish(Message *message, pid_t pid, const char *topic, const char *payload)
{
    message->mtype = BROKER_MTYPE;
    sprintf(message->mtext, "send,%ld,%s,%s", (long)pid, topic, payload);
}

void protocol_parse(const Message *message, ProtocolMessage *parsed)
{
    char command[16];
    int pid;
    sscanf(message->mtext, "%15[^,],%d,%127[^,],%1023[^\n]", command, &pid, parsed->topic,
           parsed->payload);

    if (strcmp(command, "subscribe") == 0)
    {
        parsed->command = PROTOCOL_SUBSCRIBE;
        parsed->payload[0] = '\0';
    }
    else if (strcmp(command, "unsubscribe") == 0)
    {
        parsed->command = PROTOCOL_UNSUBSCRIBE;
        parsed->payload[0] = '\0';
    }
    else
    {
        parsed->command = PROTOCOL_PUBLISH;
    }

    parsed->pid = (pid_t)pid;
}

void protocol_make_delivery(Message *message, pid_t recipient, const char *topic,
                            const char *payload)
{
    message->mtype = (long)recipient;
    sprintf(message->mtext, "topic=%s payload=%s", topic, payload);
}