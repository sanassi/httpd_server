#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../tools/tools.h"

struct global
{
    char *pid_file;
    char *log_file;
    bool log;
};

struct vhosts
{
    char *server_name;
    char *port;
    char *ip;
    char *default_file;
    char *root_dir;

    struct vhosts *next;
};

char *get_next_token(char *s, size_t *i, char *delim);

void parse_config(struct global **global, struct vhosts **vhosts, char *path,
                  int *err_no);
void check_config(struct global *g, struct vhosts *v, int *err_no);
struct vhosts *get_vhost_at(struct vhosts *v, size_t index);
#endif /* ! CONFIG_PARSER_H */
