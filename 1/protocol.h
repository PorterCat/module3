#ifndef PROTOCOL_H
#define PROTOCOL_H

enum
{
    MSG_FILE = 1,
    MSG_FINISH
};

enum
{
    CTRL_READY = 1,
    CTRL_FILE_COPIED,
    CTRL_FILE_SKIPPED,
    CTRL_FILE_ERROR
};

typedef struct
{
    int type;
    int name_length;
    int file_size;
} FileMessage;

#endif