#ifndef MESSAGE_H
#define MESSAGE_H

#define MESSAGE_TEXT_SIZE 1024
#define TOPIC_SIZE 128
#define BROKER_MTYPE 1

typedef struct
{
    long mtype;
    char mtext[MESSAGE_TEXT_SIZE];
} Message;

#endif