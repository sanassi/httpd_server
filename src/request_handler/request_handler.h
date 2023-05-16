#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <stdbool.h>
#include <stddef.h>

enum message_type
{
    REQUEST,
    RESPONSE
};

enum request_method
{
    GET,
    HEAD,
    FAIL
};

struct header
{
    struct list *fields;
    struct header *next;
};

struct http_message
{
    enum request_method meth;
    char *start_line;
    struct header **headers;
    size_t nb_header;
    size_t body_len;
    bool has_body;
    char *body;
};

struct http_message *request_handler(char buff[], size_t len, int *err_no);
void free_message(struct http_message *msg);
struct header *get_host_header(struct http_message *msg);
// struct vhosts *get_vhost(struct http_message *msg, struct vhosts *v);
char *find_path(struct http_message *msg);
void check_version(struct http_message *msg, int *err);
// void check_host(struct header *h, struct vhosts *v, int *err);
#endif /* ! REQUEST_HANDLER_H */
