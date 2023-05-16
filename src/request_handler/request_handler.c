#include "request_handler.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config_parser/config_parser.h"
#include "../server/server.h"
#include "../tools/list_tools.h"

char *go_to_cr_lf(char buff[], size_t *i, size_t *len_header, size_t len)
{
    if (buff[*i] == '\r')
        return NULL;
    char *r = strstr(buff + *i, "\r");
    if (*(r + 1) != '\n')
    {
        *len_header = 0;
        return NULL;
    }

    char *n = strstr(buff + *i, "\n");
    if (*(n - 1) != '\r')
    {
        *len_header = 0;
        return NULL;
    }
    char *cr_lf_ptr = strstr(buff + *i, "\r\n");
    if (cr_lf_ptr == NULL)
    {
        *len_header = len - *i;
    }
    else
        *len_header = cr_lf_ptr - (buff + *i);

    char *header_line = malloc(sizeof(char) * (*len_header + 1));
    size_t j = 0;

    for (; j < *len_header; j++)
    {
        header_line[j] = buff[*i + j];
    }

    *i = *i + *len_header;

    header_line[j] = 0;

    return header_line;
}

void headers_destroy(struct header *h)
{
    if (h)
    {
        headers_destroy(h->next);
        list_destroy(h->fields);
        free(h);
    }
}

void free_message(struct http_message *msg)
{
    if (!msg)
        return;
    struct header *h = *msg->headers;
    headers_destroy(h);
    free(msg->headers);
    free(msg->start_line);
    if (msg->has_body)
        free(msg->body);
    free(msg);
}

static void set_method(struct http_message *msg, char *start_line, int *err_no)
{
    char *meth;
    size_t i = 0;

    meth = get_next_token(start_line, &i, " ");
    if (strcmp(meth, "GET") == 0)
    {
        msg->meth = GET;
    }
    else if (strcmp(meth, "HEAD") == 0)
    {
        msg->meth = HEAD;
    }
    else
    {
        *err_no = E_405;
        msg->meth = FAIL;
    }

    // check if http version is ok
    char *path = get_next_token(start_line, &i, " ");
    char *version = get_next_token(start_line, &i, " ");
    if (path == NULL)
    {
        *err_no = E_400;
    }
    if (version == NULL)
    {
        *err_no = E_400;
    }
    free(path);
    free(version);
    free(meth);
}

char *string_to_lower(char *s)
{
    char *tmp = calloc(1, sizeof(char) * strlen(s) + 1);
    size_t i = 0;
    for (; i < strlen(s); i++)
        tmp[i] = tolower(s[i]);
    return tmp;
}

void check_request(struct http_message *msg, int *err_no)
{
    struct header **headers = msg->headers;
    struct header *h = *headers;
    int h_host = 0;

    while (h != NULL && h->fields != NULL)
    {
        if (h->fields != NULL)
        {
            char *host = h->fields->data;
            char *host_to_lower = string_to_lower(host);
            if (strcmp(host_to_lower, "host") == 0)
            {
                h_host = 1;
            }
            free(host_to_lower);
            if (list_length(h->fields) < 2)
            {
                *err_no = E_400;
                return;
            }
        }
        h = h->next;
    }
    //-------------------------modif-------------------------
    if (h_host != 1)
    {
        *err_no = E_400;
        return;
    }
}

void check_version(struct http_message *msg, int *err)
{
    size_t i = 0;
    char *start_line = msg->start_line;
    char *meth = get_next_token(start_line, &i, " ");
    char *path = get_next_token(start_line, &i, " ");
    char *protocol = get_next_token(start_line, &i, " ");

    if (strstr(protocol, "HTTP/") == NULL || strlen(protocol) != 8
        || protocol[6] != '.' || isdigit(protocol[5]) == 0
        || isdigit(protocol[7]) == 0)
    {
        *err = E_400;
        free(meth);
        free(path);
        free(protocol);
        return;
    }
    else
    {
        if (protocol[5] != '1' || protocol[7] != '1')
        {
            *err = E_505;
        }
    }
    free(meth);
    free(path);
    free(protocol);
}

static bool is_number(char *s)
{
    if (!s)
        return false;

    for (size_t i = 0; s[i] != '\0'; i++)
    {
        if (!(s[i] >= '0' && s[i] <= '9'))
        {
            return false;
        }
    }

    return true;
}

struct http_message *request_handler(char buff[], size_t len, int *err_no)
{
    struct http_message *msg = calloc(1, sizeof(struct http_message));
    size_t i = 0;
    size_t len_header = 0;
    // get message start line
    char *line = go_to_cr_lf(buff, &i, &len_header, len);
    if (line == NULL)
    {
        free(msg);
        *err_no = E_400;
        return msg;
    }
    struct header *headers = calloc(1, sizeof(struct header));
    msg->headers = calloc(1, sizeof(struct header *));
    *msg->headers = headers;
    msg->start_line = my_strdup(line);
    bool has_body = false;
    msg->has_body = false;
    struct header *h = headers;

    i += 2;

    while (i < len)
    {
        char *header_line = go_to_cr_lf(buff, &i, &len_header, len);
        if (header_line == NULL)
        {
            if (len_header == 0)
            {
                // TODO : check 20h23
                free(line);
                *err_no = E_400;
                free(header_line);
                return msg;
            }
            i += 2;
            continue;
        }
        i += 2;
        size_t j = 0;
        while (j < len_header)
        {
            char *token = get_next_token(header_line, &j, " :");
            if (token == NULL)
                continue;

            if (has_body)
            {
                if (!is_number(token))
                {
                    *err_no = E_400;
                    free(token);
                    break;
                }
                msg->body_len = atoi(token);
                has_body = false;
            }

            char *token_to_lower = string_to_lower(token);
            if (strcmp(token_to_lower, "content-length") == 0)
            {
                has_body = true;
                msg->has_body = true; // 17h35
            }

            h->fields = list_append(h->fields, token, strlen(token) + 1);
            free(token);
            free(token_to_lower);
        }
        // new header should be allocated inside of loop
        if (h->next == NULL)
        {
            h->next = calloc(1, sizeof(struct header));
        }
        h = h->next;
        free(header_line);
    }
    h->next = NULL;

    check_request(msg, err_no);

    if (*err_no != NO_ERROR)
    {
        free(line);
        return msg;
    }

    set_method(msg, line, err_no);

    if (*err_no != NO_ERROR)
    {
        free(line);
        return msg;
    }

    free(line);

    return msg;
}

struct header *get_host_header(struct http_message *msg)
{
    struct header *h = *msg->headers;
    while (h)
    {
        if (h->fields
            == NULL) // why ? empty header at then end (should be fixed)
        {
            break;
        }
        char *h_char = h->fields->data;
        char *h_char_to_lower = string_to_lower(h_char);
        if (strcmp(h_char_to_lower, "host") == 0)
        {
            free(h_char_to_lower);
            return h;
        }
        free(h_char_to_lower);
        h = h->next;
    }

    return NULL;
}

struct vhosts *get_vhost(struct http_message *msg, struct vhosts *v)
{
    struct header *host_header = get_host_header(msg);
    size_t len = list_length(host_header->fields);

    char *server_name = NULL;
    char *ip = NULL;
    char *port = NULL;

    if (len == 2)
    {
        server_name = host_header->fields->next->data;
    }
    else if (len == 3)
    {
        ip = host_header->fields->next->data;
        port = host_header->fields->next->next->data;
    }
    else
    {
        return NULL;
    }

    struct vhosts *tmp_v = v;
    while (tmp_v)
    {
        if (server_name != NULL)
        {
            if (strcmp(server_name, tmp_v->server_name) == 0)
            {
                return tmp_v;
            }
        }
        else
        {
            if (ip != NULL)
            {
                if (strcmp(ip, tmp_v->ip) == 0)
                    return tmp_v;
            }
            else if (port != NULL && (strcmp(port, tmp_v->port) == 0))
            {
                return tmp_v;
            }
        }
        tmp_v = tmp_v->next;
    }

    return NULL;
}

char *find_path(struct http_message *msg)
{
    char *file = msg->start_line;
    size_t i = 0;

    char *meth = get_next_token(file, &i, " ");
    char *path = get_next_token(file, &i, " ");
    char *protocol = get_next_token(file, &i, " ");
    free(protocol);
    free(meth);
    return path;
}

void message_pretty_print(struct http_message *msg)
{
    struct header *h = *msg->headers;
    size_t count = 0;
    while (h != NULL)
    {
        struct list *l = h->fields;
        count += 1;
        while (l != NULL)
        {
            char *str = l->data;
            printf("'%s'", str);
            l = l->next;
        }
        printf("\n");
        h = h->next;
    }
}
