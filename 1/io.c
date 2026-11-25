#include "io.h"
#include "protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int read_all(int fd, void *buffer, size_t size)
{
    size_t done = 0;

    while (done < size)
    {
        ssize_t n = read(fd, (char *)buffer + done, size - done);
        if (n > 0)
            done += (size_t)n;
        else if (n == -1 && errno == EINTR)
            continue;
        else
        {
            if (n == 0)
                errno = EPIPE;
            return -1;
        }
    }
    return 0;
}

int write_all(int fd, const void *buffer, size_t size)
{
    size_t done = 0;

    while (done < size)
    {
        ssize_t n = write(fd, (const char *)buffer + done, size - done);
        if (n > 0)
            done += (size_t)n;
        else if (n == -1 && errno == EINTR)
            continue;
        else
        {
            if (n == 0)
                errno = EIO;
            return -1;
        }
    }
    return 0;
}

int send_file(int data_fd, const char *filename)
{
    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1)
    {
        perror(filename);
        return 1;
    }

    struct stat info;
    size_t name_length = strlen(filename);
    if (fstat(file_fd, &info) == -1)
    {
        perror("fstat source");
        close(file_fd);
        return 1;
    }
    if (!S_ISREG(info.st_mode) || info.st_size < 0 || info.st_size > INT_MAX ||
        name_length > INT_MAX)
    {
        fprintf(stderr, "%s: unsupported file or metadata error\n", filename);
        close(file_fd);
        return 1;
    }

    FileMessage message = {MSG_FILE, (int)name_length, (int)info.st_size};
    if (write_all(data_fd, &message, sizeof(message)) == -1 ||
        write_all(data_fd, filename, name_length) == -1)
    {
        perror("write file header");
        close(file_fd);
        return -1;
    }

    char buffer[4096];
    int remaining = message.file_size;
    while (remaining > 0)
    {
        size_t chunk = remaining < (int)sizeof(buffer) ? (size_t)remaining : sizeof(buffer);
        ssize_t n = read(file_fd, buffer, chunk);
        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0)
        {
            if (n == 0)
                fprintf(stderr, "%s: unexpected end of file\n", filename);
            else
                perror("read source");
            close(file_fd);
            return -1;
        }
        if (write_all(data_fd, buffer, (size_t)n) == -1)
        {
            perror("write file data");
            close(file_fd);
            return -1;
        }
        remaining -= (int)n;
    }

    if (close(file_fd) == -1)
    {
        perror("close source");
        return -1;
    }
    return 0;
}

int receive_file(int data_fd)
{
    FileMessage message;
    if (read_all(data_fd, &message, sizeof(message)) == -1)
    {
        perror("read file header");
        return -1;
    }
    if (message.type == MSG_FINISH)
        return 0;
    if (message.type != MSG_FILE || message.name_length <= 0 || message.file_size < 0)
    {
        fprintf(stderr, "Invalid file message\n");
        return -1;
    }

    char *filename = malloc((size_t)message.name_length + 1);
    if (filename == NULL)
    {
        perror("malloc");
        return -1;
    }
    if (read_all(data_fd, filename, (size_t)message.name_length) == -1)
    {
        perror("read filename");
        free(filename);
        return -1;
    }
    filename[message.name_length] = '\0';

    size_t copy_length = (size_t)message.name_length + sizeof(".copy");
    char *copy_name = malloc(copy_length);
    if (copy_name == NULL)
    {
        perror("malloc");
        free(filename);
        return -1;
    }
    snprintf(copy_name, copy_length, "%s.copy", filename);

    int file_fd = open(copy_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int status = CTRL_FILE_COPIED;
    if (file_fd == -1)
    {
        perror(copy_name);
        status = CTRL_FILE_SKIPPED;
    }

    char buffer[4096];
    int remaining = message.file_size;
    while (remaining > 0)
    {
        size_t chunk = remaining < (int)sizeof(buffer) ? (size_t)remaining : sizeof(buffer);
        ssize_t n = read(data_fd, buffer, chunk);
        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0)
        {
            if (n == 0)
                fprintf(stderr, "Unexpected end of file data\n");
            else
                perror("read file data");
            if (file_fd != -1)
                close(file_fd);
            free(copy_name);
            free(filename);
            return -1;
        }

        if (file_fd != -1 && write_all(file_fd, buffer, (size_t)n) == -1)
        {
            perror("write copy");
            close(file_fd);
            file_fd = -1;
            status = CTRL_FILE_ERROR;
        }
        remaining -= (int)n;
    }

    if (file_fd != -1 && close(file_fd) == -1)
    {
        perror("close copy");
        status = CTRL_FILE_ERROR;
    }

    if (status == CTRL_FILE_COPIED)
        printf("Copied: %s\n", filename);

    free(copy_name);
    free(filename);

    return status;
}