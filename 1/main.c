#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "io.h"
#include "protocol.h"

int main(int argc, char *argv[])
{
    const char *pipe_name = NULL;

    int opt;

    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            pipe_name = optarg;
            break;

        default:
            return 1;
        }
    }

    if (optind == argc)
    {
        fprintf(stderr, "Usage: %s [-p channel_name] file1 [file2 ...]\n", argv[0]);
        return 1;
    }

    int data_pipe[2];
    int control_pipe[2];

    char data_fifo[256];
    char control_fifo[256];

    if (pipe_name == NULL)
    {
        if (pipe(data_pipe) == -1)
        {
            perror("pipe");
            return 1;
        }

        if (pipe(control_pipe) == -1)
        {
            perror("pipe");
            return 1;
        }
    }
    else
    {
        snprintf(data_fifo, sizeof(data_fifo), "%s.data", pipe_name);
        snprintf(control_fifo, sizeof(control_fifo), "%s.control", pipe_name);

        if (mkfifo(data_fifo, 0600) == -1)
        {
            perror("mkfifo");
            return 1;
        }

        if (mkfifo(control_fifo, 0600) == -1)
        {
            perror("mkfifo");
            unlink(data_fifo);
            return 1;
        }
    }

    int data_fd;
    int control_fd;

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork");

        if (pipe_name != NULL)
        {
            unlink(data_fifo);
            unlink(control_fifo);
        }

        return 1;
    }

    if (pid == 0) // Ребенок принимает данные
    {
        if (pipe_name == NULL)
        {
            close(data_pipe[1]);
            close(control_pipe[0]);

            data_fd = data_pipe[0];
            control_fd = control_pipe[1];
        }
        else
        {
            data_fd = open(data_fifo, O_RDONLY);
            control_fd = open(control_fifo, O_WRONLY);
        }

        int ready = CTRL_READY;

        if (write_all(control_fd, &ready, sizeof(ready)) == -1)
        {
            perror("write ready status");
            return 1;
        }

        int result;
        int child_error = 0;

        while ((result = receive_file(data_fd)) != 0)
        {
            if (result == -1)
            {
                child_error = 1;
                break;
            }

            if (write_all(control_fd, &result, sizeof(result)) == -1)
            {
                perror("write file status");
                child_error = 1;
                break;
            }

            if (result != CTRL_FILE_COPIED)
                child_error = 1;
        }

        close(data_fd);
        close(control_fd);

        return child_error ? 1 : 0;
    }
    else // Родитель отправляет данные
    {
        if (pipe_name == NULL)
        {
            close(data_pipe[0]);
            close(control_pipe[1]);

            data_fd = data_pipe[1];
            control_fd = control_pipe[0];
        }
        else
        {
            data_fd = open(data_fifo, O_WRONLY);
            control_fd = open(control_fifo, O_RDONLY);
        }

        int ready;

        if (read(control_fd, &ready, sizeof(ready)) == -1)
        {
            perror("read ready status");
            return 1;
        }

        if (ready != CTRL_READY)
        {
            fprintf(stderr, "Child is not ready\n");
            return 1;
        }

        printf("Child is ready\n");

        int transfer_error = 0;
        int file_error = 0;

        for (int i = optind; i < argc; i++)
        {
            int result = send_file(data_fd, argv[i]);

            if (result == -1)
            {
                transfer_error = 1;
                break;
            }

            if (result == 1)
            {
                file_error = 1;
                continue;
            }

            if (result == 0)
            {
                int file_status;
                if (read_all(control_fd, &file_status, sizeof(file_status)) == -1)
                {
                    perror("read file status");
                    transfer_error = 1;
                    break;
                }
                if (file_status != CTRL_FILE_COPIED)
                    file_error = 1;
            }
        }

        if (!transfer_error)
        {
            FileMessage finish = {.type = MSG_FINISH, .name_length = 0, .file_size = 0};

            if (write_all(data_fd, &finish, sizeof(finish)) == -1)
            {
                perror("write finish message");
                transfer_error = 1;
            }
        }

        close(data_fd);
        close(control_fd);

        int child_status;
        waitpid(pid, &child_status, 0);

        if (pipe_name != NULL)
        {
            unlink(data_fifo);
            unlink(control_fifo);
        }

        if (transfer_error || file_error || !WIFEXITED(child_status) ||
            WEXITSTATUS(child_status) != 0)
        {
            return 1;
        }
    }

    return 0;
}