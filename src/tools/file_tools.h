#ifndef FILE_TOOLS_H
#define FILE_TOOLS_H

#include <arpa/inet.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

bool file_is_readable(const char *file);
bool file_exists(const char *file);
bool is_dir(const char *path);
ssize_t get_file_len(char *path);
void close_fd(int cfd);
#endif /* ! FILE_TOOLS_H */
