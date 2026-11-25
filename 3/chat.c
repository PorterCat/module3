#define _POSIX_C_SOURCE 200809L

#include "chat.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int chat_open(Chat *chat, const char *base_name)
{
    size_t name_len = strlen(base_name);

    if (base_name[0] != '/' || name_len < 2 || strchr(base_name + 1, '/') != NULL ||
        name_len > sizeof(chat->queue1_name) - 3)
    {
        fprintf(stderr, "Invalid queue name\n");
        return -1;
    }

    snprintf(chat->queue1_name, sizeof(chat->queue1_name), "%s_1", base_name);
    snprintf(chat->queue2_name, sizeof(chat->queue2_name), "%s_2", base_name);

    struct mq_attr attr = {
        .mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = CHAT_MESSAGE_SIZE, .mq_curmsgs = 0};

    mqd_t queue1 = mq_open(chat->queue1_name, O_CREAT | O_EXCL | O_RDWR, 0666, &attr);

    if (queue1 != (mqd_t)-1)
    {
        mqd_t queue2 = mq_open(chat->queue2_name, O_CREAT | O_EXCL | O_RDWR, 0666, &attr);

        if (queue2 == (mqd_t)-1)
        {
            perror("mq_open queue2");

            mq_close(queue1);
            mq_unlink(chat->queue1_name);

            return -1;
        }

        chat->is_creator = 1;
        chat->receive_queue = queue1;
        chat->send_queue = queue2;

        printf("Created queues.\n");
        printf("Receiving: %s\n", chat->queue1_name);
        printf("Sending: %s\n", chat->queue2_name);

        return 0;
    }

    if (errno != EEXIST)
    {
        perror("mq_open queue1");
        return -1;
    }

    queue1 = mq_open(chat->queue1_name, O_RDWR);

    if (queue1 == (mqd_t)-1)
    {
        perror("mq_open queue1");
        return -1;
    }

    mqd_t queue2 = mq_open(chat->queue2_name, O_RDWR);

    if (queue2 == (mqd_t)-1)
    {
        perror("mq_open queue2");
        mq_close(queue1);
        return -1;
    }

    chat->is_creator = 0;
    chat->send_queue = queue1;
    chat->receive_queue = queue2;

    printf("Connected to existing queues.\n");
    printf("Receiving: %s\n", chat->queue2_name);
    printf("Sending: %s\n", chat->queue1_name);

    return 0;
}

void chat_run(Chat *chat, volatile sig_atomic_t *stop_requested)
{
    struct pollfd descriptors[2] = {{.fd = STDIN_FILENO, .events = POLLIN},
                                    {.fd = (int)chat->receive_queue, .events = POLLIN}};

    int peer_finished = 0;

    printf("Enter messages. Ctrl+C or Ctrl+D to exit.\n> ");
    fflush(stdout);

    while (!*stop_requested)
    {
        int ready = poll(descriptors, 2, -1);

        if (ready == -1)
        {
            if (errno == EINTR)
                continue;

            perror("poll");
            break;
        }

        if (descriptors[1].revents & (POLLERR | POLLNVAL))
        {
            fprintf(stderr, "Incoming queue error\n");
            break;
        }

        if (descriptors[1].revents & POLLIN)
        {
            char message[CHAT_MESSAGE_SIZE];
            unsigned int priority;

            ssize_t size = mq_receive(chat->receive_queue, message, sizeof(message), &priority);

            if (size == -1)
            {
                if (errno == EINTR)
                    continue;

                perror("mq_receive");
                break;
            }

            if (priority == CHAT_STOP_PRIORITY)
            {
                printf("\nPeer finished the chat.\n");
                peer_finished = 1;
                break;
            }

            if ((size_t)size < sizeof(message))
                message[size] = '\0';
            else
                message[sizeof(message) - 1] = '\0';

            printf("\nPeer: %s> ", message);
            fflush(stdout);
        }

        if (descriptors[0].revents & (POLLIN | POLLHUP))
        {
            char message[CHAT_MESSAGE_SIZE];

            if (fgets(message, sizeof(message), stdin) == NULL)
            {
                if (*stop_requested)
                    break;

                if (ferror(stdin) && errno == EINTR)
                {
                    clearerr(stdin);
                    continue;
                }

                break;
            }

            if (mq_send(chat->send_queue, message, strlen(message) + 1, CHAT_MESSAGE_PRIORITY) ==
                -1)
            {
                perror("mq_send");
                break;
            }

            printf("> ");
            fflush(stdout);
        }
    }

    if (!peer_finished)
    {
        const char stop_message[] = "END";

        if (mq_send(chat->send_queue, stop_message, sizeof(stop_message), CHAT_STOP_PRIORITY) == -1)
        {
            perror("mq_send stop");
        }
    }
}

void chat_close(Chat *chat)
{
    if (chat->send_queue != (mqd_t)-1)
        mq_close(chat->send_queue);

    if (chat->receive_queue != (mqd_t)-1)
        mq_close(chat->receive_queue);

    if (chat->is_creator)
    {
        mq_unlink(chat->queue1_name);
        mq_unlink(chat->queue2_name);
    }
}