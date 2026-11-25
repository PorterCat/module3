#include "chat.h"

#include <stdio.h>
#include <signal.h>

volatile sig_atomic_t stop_requested = 0;

void handle_sigint(int signal_number)
{
    stop_requested = (signal_number == SIGINT);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s /queue_name\n", argv[0]);
        return 1;
    }

    if (signal(SIGINT, handle_sigint) == SIG_ERR)
    {
        perror("signal");
        return 1;
    }

    setvbuf(stdin, NULL, _IONBF, 0);

    Chat chat = {.send_queue = (mqd_t)-1, .receive_queue = (mqd_t)-1, .is_creator = 0};

    if (chat_open(&chat, argv[1]) == -1)
        return 1;

    chat_run(&chat, &stop_requested);
    chat_close(&chat);

    return 0;
}