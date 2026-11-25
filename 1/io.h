#ifndef IO_H
#define IO_H

#include <stddef.h>

int read_all(int fd, void *buffer, size_t size);
int write_all(int fd, const void *buffer, size_t size);

int send_file(int data_fd, const char *filename);
int receive_file(int data_fd);

#endif