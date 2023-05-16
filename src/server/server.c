#define _POSIX_C_SOURCE 200112L
//#define NSIGMAX 5
#include "server.h"

// TODO : create include.h file
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

#include "../request_handler/request_handler.h"
#include "../tools/file_tools.h"
#include "../tools/list_tools.h"
#include "../tools/tools.h"
#include "log.h"

#define BUFFER_SIZE 4096

bool stop_loop = false;
bool reload_serv = false;
char *config_global_path;

static void log_request(struct global *g, char *server_name, int meth, char *ip,
                        char *target, int *err)
{
    if (g->log)
    {
        char log_msg[500];
        char *date = get_date();
        char *req_type = NULL;

        if (*err != E_400)
        {
            if (meth == GET)
                req_type = strdup("GET\0");
            else
                req_type = strdup("HEAD\0");
            sprintf(log_msg, "%s [%s] received %s on '%s' from %s", date,
                    server_name, req_type, target, ip);
        }
        else
        {
            sprintf(log_msg, "%s [%s] received Bad Request from %s", date,
                    server_name, ip);
        }
        if (g->log_file == NULL)
        {
            printf("%s\n", log_msg);
        }
        else
        {
            FILE *fp = fopen(g->log_file, "a+");
            if (fp != NULL)
            {
                fputs(log_msg, fp);
                fputs("\n", fp);
                fclose(fp);
            }
        }
        free(date);
        free(req_type);
    }
}

// too many arguments
static void log_response(struct global *g, char *server_name, int code,
                         char *ip, int meth, char *target)
{
    if (g->log)
    {
        char log_msg[500];
        char *date = get_date();
        // not needed with sprintf
        char *req_type = NULL;

        if (code != E_400 && code != E_405)
        {
            if (meth == GET)
                req_type = strdup("GET\0");
            else
                req_type = strdup("HEAD\0");
            sprintf(log_msg, "%s [%s] responding with %i to %s for %s on '%s'",
                    date, server_name, code, ip, req_type, target);
        }
        else if (code == E_400)
        {
            sprintf(log_msg, "%s [%s] responding with %i to %s", date,
                    server_name, code, ip);
        }
        else if (code == E_405)
        {
            sprintf(log_msg,
                    "%s [%s] responding with %i to %s for UNKNOWN on '%s'",
                    date, server_name, code, ip, target);
        }
        if (g->log_file == NULL)
        {
            printf("%s\n", log_msg);
        }
        else
        {
            FILE *fp = fopen(g->log_file, "a+");
            if (fp != NULL)
            {
                fputs(log_msg, fp);
                fputs("\n", fp);
                fclose(fp);
            }
        }
        free(date);
        free(req_type);
    }
}

static int make_socket_non_blocking(int sfd)
{
    int flags, s;

    flags = fcntl(sfd, f_getfl, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }

    flags |= o_nonblock;
    s = fcntl(sfd, f_setfl, flags);
    if (s == -1)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}
static void int_handler(int sig)
{
    if (sig == SIGINT)
    {
        stop_loop = true;
    }
}

static void reload_handler(int sig)
{
    if (sig == SIGUSR1)
    {
        reload_serv = true;
    }
}

static void init_sigaction(void (*handler)(int), int sig)
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, NULL);
}

// TODO : move func below to tools

ssize_t my_sand_file(int file_fd, int out_fd, char *path)
{
    ssize_t len = get_file_len(path);
    if (len == -1)
    {
        return -1;
    }
    off_t off = 0;
    return sendfile(out_fd, file_fd, &off, len);
}

static int create_and_bind(char *ip, char *port)
{
    struct addrinfo *result;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(ip, port, &hints, &result);

    struct addrinfo *p;
    int on = 1;

    int sfd;
    for (p = result; p != NULL; p = p->ai_next)
    {
        sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | 15, &on, sizeof(int))
            < 0)
        {
            errx(1, "error : setsockopt()");
        }

        err = bind(sfd, p->ai_addr, p->ai_addrlen);

        if (err == 0)
            break;
        else
            close(sfd);
    }

    make_socket_non_blocking(sfd);

    freeaddrinfo(result);

    return sfd;
}

static void send_error_msg(int err_code, int cfd)
{
    char *err_str;
    switch (err_code)
    {
    case E_403:
        err_str = strdup("HTTP/1.1 403 Forbidden\r\n");
        break;
    case E_404:
        err_str = strdup("HTTP/1.1 404 Not Found\r\n");
        break;
    case E_405:
        err_str = strdup("HTTP/1.1 405 Method Not Allowed\r\n");
        break;
    case E_505:
        err_str = strdup("HTTP/1.1 505 Version Not Supported\r\n");
        break;
    case E_400:
        err_str = strdup("HTTP/1.1 400 Bad request\r\n");
    }

    // char *err_msg = "Your request is not OK";// check if \0 included
    // int len = strlen(err_msg);
    int len = 0;

    char content_header[500];
    sprintf(content_header, "Content-Length: %i\r\n", len);

    char *date = get_date();
    char *con_close = "Connection: close\r\n";

    char *date_header = "Date: ";
    err_str = my_str_cat(err_str, date_header, strlen(date_header));
    err_str = my_str_cat(err_str, date, strlen(date));
    err_str = my_str_cat(err_str, "\r\n", 2);
    err_str = my_str_cat(err_str, content_header, strlen(content_header));
    err_str = my_str_cat(err_str, con_close, strlen(con_close));
    err_str = my_str_cat(err_str, "\r\n", 2);
    // err_str = my_str_cat(err_str, err_msg, len);

    send(cfd, err_str, strlen(err_str), MSG_NOSIGNAL);

    free(date);
    free(err_str);
}

static char *get_target_path(struct http_message *msg, struct vhosts *v,
                             int *err_no)
{
    char *target = find_path(msg);
    // 21h24
    if (target && *target != '/') // TODO : remove '/'
    {
        free(target);
        *err_no = E_400;
        return NULL;
    }

    char *target_path = strdup(v->root_dir);
    target_path = my_str_cat(target_path, target, strlen(target));

    if (file_exists(target_path))
    {
        if (!file_is_readable(target_path))
        {
            *err_no = E_403;
        }
    }
    else
    {
        if (!is_dir(target_path))
        {
            *err_no = E_404;
        }
        else
        {
            char *default_file = strdup(v->root_dir);
            default_file = my_str_cat(default_file, "/", 1);
            default_file = my_str_cat(default_file, v->default_file,
                                      strlen(v->default_file));
            if (!file_exists(default_file))
            {
                *err_no = E_404; // check
            }
            else
            {
                if (!file_is_readable(default_file))
                {
                    *err_no = E_403;
                }
            }
            free(target);
            free(target_path);
            return default_file;
        }
    }

    free(target);
    return target_path;
}

// TODO : use sprintf !!
void send_ok_message(int cfd, char *target_path, int meth)
{
    char *ok_msg = NULL;
    char *status = "HTTP/1.1 200 OK\r\n";

    char *date = get_date();
    date = my_str_cat(date, "\r\n", 2);

    char *content = strdup("Content-Length: ");
    char *len_as_str = my_itoa(get_file_len(target_path));

    content = my_str_cat(content, len_as_str, strlen(len_as_str));
    content = my_str_cat(content, "\r\n", 2);
    char *con_close = "Connection: close\r\n";

    ok_msg = my_str_cat(ok_msg, status, strlen(status));
    char *date_header = "Date: ";
    ok_msg = my_str_cat(ok_msg, date_header, strlen(date_header));
    ok_msg = my_str_cat(ok_msg, date, strlen(date));
    ok_msg = my_str_cat(ok_msg, content, strlen(content));
    ok_msg = my_str_cat(ok_msg, con_close, strlen(con_close));

    ok_msg = my_str_cat(ok_msg, "\r\n", 2);

    send(cfd, ok_msg, strlen(ok_msg), MSG_NOSIGNAL);
    // printf("--------------------------\n");

    if (meth == GET)
    {
        // test send file
        int filefd = open(target_path, O_RDONLY);
        my_sand_file(filefd, cfd, target_path);
        close(filefd);
    }

    free(ok_msg);
    free(date);
    free(content);
    free(len_as_str);
}

// TODO : free handle error var in free_resources
//
static void handle_error(int err_no, int cfd)
{
    send_error_msg(err_no, cfd);
}

static void free_resources(struct http_message *msg, char *msg_cpy,
                           char *full_msg, char *target_path)
{
    if (full_msg)
        free(full_msg);
    if (msg_cpy)
        free(msg_cpy);
    if (target_path)
        free(target_path);
    free_message(msg);
    msg = NULL;
}

static void get_body(struct http_message *msg, char *body_start, char *buf,
                     int cfd)
{
    if (msg->has_body)
    {
        msg->body = calloc(1, msg->body_len * sizeof(char) + 1);
        size_t idx = 0;
        while (*body_start != '\0' && idx < msg->body_len)
        {
            msg->body[idx] = *body_start;
            body_start += 1;
            idx += 1;
        }
        ssize_t r;

        while (idx < msg->body_len)
        {
            r = recv(cfd, buf, BUFFER_SIZE, 0);
            for (int i = 0; i < r && idx + i < msg->body_len; i++)
            {
                msg->body[idx + i] = buf[i];
            }
            idx += r;
        }
    }
}
static void check_host(struct http_message *msg, struct vhosts *v, int *err)
{
    struct header *h = get_host_header(msg);
    if (h == NULL)
    {
        *err = E_400;
        return;
    }
    size_t len = list_length(h->fields);

    // Host: IP || Host: server_name
    if (len == 2)
    {
        char *val = h->fields->next->data;
        if (!(strcmp(val, v->ip) == 0 || strcmp(val, v->server_name) == 0))
        {
            *err = E_400;
            return;
        }
    }
    else if (len == 3) // Host: IP:port || Host: server_name:port
    {
        char *val1 = h->fields->next->data;
        char *val2 = h->fields->next->next->data;

        if (!(strcmp(val1, v->ip) == 0 || strcmp(val1, v->server_name) == 0))
        {
            *err = E_400;
            return;
        }
        if (!(strcmp(val2, v->port) == 0))
        {
            *err = E_400;
            return;
        }
    }
}

static void loop_server(int sfd, struct global *g, struct vhosts *vhosts,
                        int index)
{
    init_sigaction(reload_handler, SIGUSR1);
    init_sigaction(int_handler, SIGINT);
    // added 22h51
    while (!stop_loop)
    {
        if (reload_serv)
        {
            free(g->log_file);
            free(g->pid_file);
            free(g);
            free_vhosts(vhosts);
            g = NULL;
            vhosts = NULL;
            int err = 0;
            parse_config(&g, &vhosts, config_global_path, &err);
            reload_serv = false;
        }
        // read from buffer and concatenate result in string (full_msg)
        // which will store our http_message
        char *full_msg = NULL;

        // put in separate function
        struct sockaddr cli_addr;
        socklen_t slen;
        int cfd = accept(sfd, &cli_addr, &slen);
        if (cfd == -1)
            continue;
        char addr_str[INET_ADDRSTRLEN];

        void *tmp = &cli_addr;
        struct sockaddr_in *in = tmp;

        inet_ntop(AF_INET, &(in->sin_addr), addr_str, INET_ADDRSTRLEN);

        ssize_t r = 0;
        char buf[BUFFER_SIZE];

        char *body_start = NULL;
        char *end = NULL;
        while ((r = recv(cfd, buf, BUFFER_SIZE, 0)) > 0)
        {
            full_msg = my_str_cat(full_msg, buf, r);
            end = strstr(full_msg, "\r\n\r\n");
            if (end)
                break;
        }

        if (end == NULL)
            break;
        char *msg_cpy = strdup(full_msg);
        char *end_cpy = strstr(msg_cpy, "\r\n\r\n");
        *(end_cpy + 4) = '\0';
        body_start = end + 4;
        int err_no = NO_ERROR;
        struct http_message *msg =
            request_handler(msg_cpy, strlen(msg_cpy), &err_no);

        struct vhosts *v = get_vhost_at(vhosts, index);

        // bad request
        if (err_no != NO_ERROR)
        {
            char *target = NULL;
            if (err_no != E_400)
                check_version(msg, &err_no);

            if (err_no != E_400)
                target = get_target_path(msg, v, &err_no);

            log_request(g, v->server_name, 0, addr_str, target, &err_no);
            log_response(g, v->server_name, err_no, addr_str, 0, target);
            handle_error(err_no, cfd);
            free_resources(msg, msg_cpy, full_msg, NULL);
            // 19h39
            free(target);
            close_fd(cfd);
            continue;
        }

        get_body(msg, body_start, buf, cfd);

        err_no = 0;
        char *target_path = get_target_path(msg, v, &err_no);
        // added 17h20

        // TODO : rename find_path to get_target
        char *target = find_path(msg);

        int err_no_3 = 0;
        check_host(msg, v, &err_no_3);
        int err_no_2 = 0;
        check_version(msg, &err_no_2);

        if (err_no != NO_ERROR || err_no_2 != NO_ERROR || err_no_3 != NO_ERROR)
        {
            err_no = ((err_no == E_400) || (err_no_2 == E_400)
                      || (err_no_3 == E_400))
                ? E_400
                : err_no;
            if (err_no == 0)
                err_no = err_no_2;
            if (err_no == 0)
                err_no = err_no_3;
            log_request(g, v->server_name, msg->meth, addr_str, target,
                        &err_no);
            log_response(g, v->server_name, err_no, addr_str, msg->meth,
                         target);
            handle_error(err_no, cfd);
            free_resources(msg, msg_cpy, full_msg, target_path);
            free(target);
            close_fd(cfd);
            continue;
        }

        log_request(g, v->server_name, msg->meth, addr_str, target, &err_no);
        log_response(g, v->server_name, 200, addr_str, msg->meth, target);
        free(target);
        send_ok_message(cfd, target_path, msg->meth);
        free_resources(msg, msg_cpy, full_msg, target_path);
        close_fd(cfd);
    }
    free(config_global_path);
}

// foreach vhost in config file : create and bind
static int *bind_and_get_vsfd(struct vhosts *v, int *nb_hosts)
{
    struct vhosts *tmp = v;
    *nb_hosts = 0;

    int *vsfd = malloc(sizeof(int));

    while (tmp)
    {
        *nb_hosts += 1;
        vsfd = realloc(vsfd, *nb_hosts * sizeof(int));
        vsfd[*nb_hosts - 1] = create_and_bind(tmp->ip, tmp->port);
        tmp = tmp->next;
    }
    reload_serv = false;

    return vsfd;
}

int launch(struct global *g, struct vhosts *vhosts)
{
    int nb_hosts;
    int *hosts_sfd = bind_and_get_vsfd(vhosts, &nb_hosts);

    int index = 0;

    int sfd = hosts_sfd[index];

    for (int i = 0; i < nb_hosts; i++)
    {
        int err = listen(hosts_sfd[i], 5);
        if (err == -1)
        {
            close(sfd);
            errx(EXIT_FAILURE, "error : listen()");
        }
    }

    listen(hosts_sfd[index], 5);

    free(hosts_sfd);
    loop_server(sfd, g, vhosts, index);
    close(sfd);

    return 0;
}
