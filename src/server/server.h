#ifndef SERVER_H
#define SERVER_H

#include <err.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../tools/tools.h"

struct server
{
    char *pid_file;
};
enum error_code
{
    NO_ERROR = 0,
    E_400 = 400,
    E_403 = 403,
    E_404 = 404,
    E_405 = 405,
    E_505 = 505,
};

// int launch(char *ip, char *port, struct global *g, struct vhosts *v);
int launch(struct global *g, struct vhosts *v);
ssize_t my_sand_file(int file_fd, int out_fd, char *path);
#endif /* ! SERVER_H */
