#ifndef TOOLS_H
#define TOOLS_H

#include "../config_parser/config_parser.h"

int is_delim(char c, const char *delim);

char *my_strdup(char *value);

struct vhosts *insert_list(struct vhosts *vhosts, struct vhosts *vhosts_new);
void free_vhosts(struct vhosts *v);
char *my_itoa(int value);
char *my_str_cat(char *s1, char *s2, size_t len_s2);
#endif /* ! TOOLS_H */
