#include "config_parser.h"

#include "../tools/tools.h"

char *get_next_token(char *s, size_t *i, char *delim)
{
    size_t len = strlen(s);
    char *token;

    for (size_t j = *i; j < len && s[j] != '\0'; j++)
    {
        if (isspace(s[j]) || is_delim(s[j], delim))
        {
            *i = j;
            continue;
        }

        size_t word_start_idx = j;
        while (word_start_idx < len && !is_delim(s[word_start_idx], delim))
        {
            word_start_idx += 1;
        }

        *i = word_start_idx;
        token = malloc((word_start_idx - j + 1) * sizeof(char));
        size_t word_len = word_start_idx - j;

        size_t k = 0;
        for (; k < word_len && k + j < len; k++)
        {
            token[k] = s[k + j];
        }
        if (token[k - 1] == '\n')
        {
            token[k - 1] = '\0';
        }
        token[k] = '\0';

        return token;
    }

    *i = len;
    return NULL;
}

static void set_global(struct global *global, char *key, char *value)
{
    // global->log = true;
    if (strcmp(key, "pid_file") == 0)
        global->pid_file = my_strdup(value);

    else if (strcmp(key, "log_file") == 0)
        global->log_file = my_strdup(value);

    else if (strcmp(key, "log") == 0)
    {
        if (strcmp(value, "true") == 0)
            global->log = true;
        else
            global->log = false;
    }
}

static void set_vhosts(struct vhosts *vhosts, char *key, char *value)
{
    if (strcmp(key, "server_name") == 0)
        vhosts->server_name = my_strdup(value);

    else if (strcmp(key, "port") == 0)
        vhosts->port = my_strdup(value);

    else if (strcmp(key, "ip") == 0)
        vhosts->ip = my_strdup(value);

    else if (strcmp(key, "default_file") == 0)
    {
        vhosts->default_file = realloc(vhosts->default_file, strlen(value) + 1);
        strcpy(vhosts->default_file, value);
    }

    else if (strcmp(key, "root_dir") == 0)
        vhosts->root_dir = my_strdup(value);
}

void parse_config(struct global **global, struct vhosts **vhosts, char *path,
                  int *err_no)
{
    if (path == NULL)
    {
        *err_no = 1;
        return;
    }
    FILE *fp = fopen(path, "r");
    // TODO : set err_no when fopen fails
    if (fp == NULL)
    {
        *err_no = 1;
        return;
    }

    char *line = NULL;
    size_t len;
    ssize_t n;
    size_t i = 0;
    bool is_global = false;
    struct vhosts *cpy = *vhosts;

    while ((n = getline(&line, &len, fp)) != -1)
    {
        i = 0;
        char *token = get_next_token(line, &i, " =");

        if (token == NULL)
            continue;

        if (strcmp(token, "[global]") == 0)
        {
            *global = calloc(1, sizeof(struct global));
            is_global = true;
            (*global)->log = true;
            free(token);

            continue;
        }

        if (strcmp(token, "[[vhosts]]") == 0)
        {
            struct vhosts *vhosts_new = calloc(1, sizeof(struct vhosts));
            vhosts_new->default_file = strdup("index.html\0");
            *vhosts = insert_list(*vhosts, vhosts_new);
            cpy = vhosts_new;
            is_global = false;
            free(token);

            continue;
        }

        char *value = get_next_token(line, &i, " =");
        if (value == NULL)
        {
            *err_no = 2;
            free(token);
            break;
        }

        if (is_global)
        {
            set_global(*global, token, value);
        }
        else
        {
            set_vhosts(cpy, token, value);
        }

        free(token);
        free(value);
    }

    free(line);
    fclose(fp);
}

void check_config(struct global *g, struct vhosts *v, int *err_no)
{
    // check if madatory keys are present
    if (g->pid_file == NULL)
    {
        *err_no = 2;
        return;
    }

    struct vhosts *tmp = v;

    while (tmp)
    {
        if (tmp->server_name == NULL || tmp->port == NULL || tmp->ip == NULL
            || tmp->root_dir == NULL)
        {
            *err_no = 2;
            return;
        }

        tmp = tmp->next;
    }
}

struct vhosts *get_vhost_at(struct vhosts *v, size_t index)
{
    struct vhosts *res = v;
    while (index != 0)
    {
        res = res->next;
        if (res == NULL)
        {
            return NULL;
        }
        index -= 1;
    }

    return res;
}
