#include "tools.h"

#include "../config_parser/config_parser.h"

int is_delim(char c, const char *delim)
{
    for (size_t i = 0; i < strlen(delim); i++)
    {
        if (delim[i] == c)
        {
            return 1;
        }
    }

    return 0;
}

char *my_strdup(char *value)
{
    int len = strlen(value) + 1;
    char *res = malloc(sizeof(char) * len);
    strcpy(res, value);
    res[len - 1] = 0;

    return res;
}

struct vhosts *insert_list(struct vhosts *vhosts, struct vhosts *vhosts_new)
{
    if (vhosts == NULL)
    {
        vhosts = vhosts_new;
        return vhosts;
    }
    struct vhosts *current = vhosts;

    while (current->next != NULL)
    {
        current = current->next;
    }

    current->next = vhosts_new;

    return vhosts;
}
void free_global(struct global *g)
{
    if (g)
    {
        free(g->pid_file);
        free(g->log_file);
        free(g);
    }
}
void free_vhosts(struct vhosts *v)
{
    if (v)
    {
        free_vhosts(v->next);
        free(v->server_name);
        free(v->ip);
        free(v->port);
        free(v->default_file);
        free(v->root_dir);
        free(v);
    }
}

static int get_number_len(int n)
{
    if (n < 10)
    {
        return 1;
    }
    int len = 0;

    while (n != 0)
    {
        len += 1;
        n = n / 10;
    }

    return len;
}

static int get_nth_digit(int n, int nth)
{
    while (nth > 0 && n > 1)
    {
        nth -= 1;
        n = n / 10;
    }

    return n % 10;
}

// only for positive values
char *my_itoa(int value)
{
    char *s = malloc(get_number_len(value) * sizeof(char) + 1);
    if (value == 0)
    {
        s[0] = '0';
        s[1] = '\0';
        return s;
    }
    int sign = 1;
    if (value < 0)
    {
        sign = -1;
        value *= -1;
    }

    int start = 0;
    if (sign < 0)
    {
        s[start] = '-';
        start += 1;
    }

    int digit_len = get_number_len(value) - 1;

    while (digit_len >= 0)
    {
        s[start] = get_nth_digit(value, digit_len) + '0';
        digit_len -= 1;
        start += 1;
    }

    s[start] = '\0';
    return s;
}

char *my_str_cat(char *s1, char *s2, size_t len_s2)
{
    if (s1 == NULL)
    {
        s1 = strndup(s2, len_s2);
    }
    else
    {
        s1 = realloc(s1, (strlen(s1) + len_s2 + 1) * sizeof(char));
        strncat(s1, s2, len_s2);
    }
    return s1;
}
