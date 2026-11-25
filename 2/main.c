#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "message.h"
#include "publishers.h"
#include "queue.h"
#include "subscriptions.h"

volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    stop_requested = (signal_number == SIGINT);
}

static void remove_dead_subscriber(Subscription **subscriptions, pid_t pid)
{
    Subscription **link = subscriptions;
    while (*link != NULL)
    {
        Subscription *item = *link;
        if (item->pid == pid)
        {
            *link = item->next;
            free(item);
        }
        else
        {
            link = &item->next;
        }
    }
}

int run_broker()
{
    int queue_id = queue_create();
    if (queue_id == -1)
    {
        if (errno == EEXIST)
            fprintf(stderr, "Broker is already running\n");
        else
            perror("queue_create");
        return 1;
    }

    Subscription *subscriptions = NULL;
    Publisher *publishers = NULL;
    
    printf("Broker started. Queue ID: %d\nPress Ctrl+C to stop broker\n", queue_id);

    while (!stop_requested)
    {
        Message message;
        ssize_t received = queue_receive(queue_id, &message, BROKER_MTYPE);
        if (received == -1)
        {
            if (errno == EINTR && stop_requested)
                break;
            if (errno == EIDRM || errno == EINVAL)
                break;
            if (errno == EINTR)
                continue;
            perror("msgrcv");
            break;
        }

        long pid_value;
        char verb[16];
        char topic[TOPIC_SIZE];
        char payload[MESSAGE_TEXT_SIZE];
        sscanf(message.mtext, "%15[^,],%ld,%127[^,],%1023[^\n]", verb, &pid_value, topic, payload);

        pid_t pid = (pid_t)pid_value;
        if (strcmp(verb, "subscribe") == 0)
        {
            if (subscriptions_add(&subscriptions, pid, topic) == -1)
                perror("subscriptions_add");
            else
                printf("Subscriber %ld subscribed to %s\n", pid_value, topic);
        }
        else if (strcmp(verb, "unsubscribe") == 0)
        {
            subscriptions_remove(&subscriptions, pid, topic);
        }
        else
        {
            publishers_add(&publishers, pid);

            for (Subscription *item = subscriptions; item != NULL; item = item->next)
            {
                if (strcmp(item->topic, topic) != 0)
                    continue;
                Message delivery;
                delivery.mtype = (long)item->pid;
                size_t topic_length = strlen(topic);
                size_t payload_length = strlen(payload);
                if (topic_length + payload_length + 16 > sizeof(delivery.mtext))
                    continue;

                memcpy(delivery.mtext, "topic=", 6);
                memcpy(delivery.mtext + 6, topic, topic_length);
                memcpy(delivery.mtext + 6 + topic_length, " payload=", 9);
                memcpy(delivery.mtext + 15 + topic_length, payload, payload_length + 1);
                
                if (queue_send(queue_id, &delivery) == -1)
                {
                    if (errno == EIDRM || errno == EINVAL)
                        stop_requested = 1;
                    else if (errno == EAGAIN)
                        remove_dead_subscriber(&subscriptions, item->pid);
                    else
                        perror("msgsnd delivery");
                    break;
                }
            }
        }
    }

    queue_remove(queue_id);

    for (Subscription *item = subscriptions; item != NULL; item = item->next)
        kill(item->pid, SIGINT);
    
    for (Publisher *item = publishers; item != NULL; item = item->next)
        kill(item->pid, SIGINT);

    subscriptions_free(&subscriptions);
    publishers_free(&publishers);

    printf("Broker stopped and queue removed\n");
    return 0;
}

int run_publisher(int queue_id, const char *topic)
{
    char payload[MESSAGE_TEXT_SIZE];
    pid_t pid = getpid();
    printf("Publisher %ld connected to topic %s. Enter payload; blank line exits.\n", (long)pid,
           topic);

    while (!stop_requested)
    {
        if (fgets(payload, sizeof(payload), stdin) == NULL)
            break;
        payload[strcspn(payload, "\n")] = '\0';
        if (payload[0] == '\0')
            break;

        Message message;
        message.mtype = BROKER_MTYPE;
        char prefix[TOPIC_SIZE + 32];
        int prefix_length = snprintf(prefix, sizeof(prefix), "send,%ld,%s,", (long)pid, topic);
        size_t payload_length = strlen(payload);
        if (prefix_length < 0 || (size_t)prefix_length + payload_length >= sizeof(message.mtext))
        {
            fprintf(stderr, "Message is too long\n");
            continue;
        }
        memcpy(message.mtext, prefix, (size_t)prefix_length);
        memcpy(message.mtext + prefix_length, payload, payload_length + 1);
        if (queue_send(queue_id, &message) == -1)
        {
            if (errno == EIDRM || errno == EINVAL)
                break;
            if (errno == EINTR && stop_requested)
                break;
            perror("msgsnd");
            return 1;
        }
    }
    return 0;
}

static int send_control(int queue_id, const char *verb, pid_t pid, const char *topic)
{
    Message message;
    message.mtype = BROKER_MTYPE;
    snprintf(message.mtext, sizeof(message.mtext), "%s,%ld,%s", verb, (long)pid, topic);
    return queue_send(queue_id, &message);
}

int run_subscriber(int queue_id, int argc, char *argv[], int first_topic)
{
    pid_t pid = getpid();
    int subscribed = 0;
    printf("Subscriber %ld connected\n", (long)pid);

    for (int i = first_topic; i < argc; i++)
    {
        if (send_control(queue_id, "subscribe", pid, argv[i]) == -1)
        {
            if (errno == EIDRM || errno == EINVAL)
                return 1;
            perror("subscribe");
            return 1;
        }
        subscribed++;
    }

    while (!stop_requested)
    {
        Message message;
        ssize_t received = queue_receive(queue_id, &message, (long)pid);
        if (received == -1)
        {
            if (errno == EINTR && stop_requested)
                break;
            if (errno == EIDRM || errno == EINVAL)
                break;
            if (errno == EINTR)
                continue;
            perror("msgrcv");
            break;
        }
        printf("Subscriber %ld received: %s\n", (long)pid, message.mtext);
        fflush(stdout);
    }

    if (subscribed > 0)
    {
        for (int i = first_topic; i < argc; i++)
        {
            if (send_control(queue_id, "unsubscribe", pid, argv[i]) == -1 && errno != EIDRM &&
                errno != EINVAL && errno != EINTR)
                perror("unsubscribe");
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int opt;
    int queue_id;
    char mode = '\0';
    char *topic = NULL;

    while ((opt = getopt(argc, argv, "bp:s")) != -1)
    {
        if (mode != '\0')
        {
            fprintf(stderr, "Only one mode can be selected\n");
            return 1;
        }
        mode = (char)opt;
        if (opt == 'p')
            topic = optarg;
    }

    if (signal(SIGINT, handle_signal) == SIG_ERR)
    {
        perror("signal");
        return 1;
    }

    switch (mode)
    {
    case 'b':
        if (optind != argc)
        {
            fprintf(stderr, "Usage: %s -b\n", argv[0]);
            return 1;
        }
        return run_broker();
    case 'p':
        if (optind != argc || topic == NULL || strlen(topic) >= TOPIC_SIZE)
        {
            fprintf(stderr, "Usage: %s -p <topic>\n", argv[0]);
            return 1;
        }
        queue_id = queue_open();
        if (queue_id == -1)
        {
            perror("queue_open");
            return 1;
        }
        return run_publisher(queue_id, topic);
    case 's':
        if (optind == argc)
        {
            fprintf(stderr, "Usage: %s -s <topic> [...]\n", argv[0]);
            return 1;
        }
        queue_id = queue_open();
        if (queue_id == -1)
        {
            perror("queue_open");
            return 1;
        }
        return run_subscriber(queue_id, argc, argv, optind);
    default:
        fprintf(stderr, "Usage: %s -b | -p <topic> | -s <topic> [...]\n", argv[0]);
        return 1;
    }
}