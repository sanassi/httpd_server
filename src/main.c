#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config_parser/config_parser.h"
#include "request_handler/request_handler.h"
#include "server/server.h"
#include "tools/list_tools.h"
#include "tools/tools.h"

#define NSIGMAX 5

char *config_global_path;

static void free_global(struct global *g)
{
    if (!g)
        return;
    free(g->pid_file);
    free(g->log_file);
    free(g);
}

// TODO : put args handling in separate file
struct args
{
    bool dry_run;

    bool daemon;
    bool start;
    bool stop;
    bool reload;
    bool restart;
    char *config_path;
};

struct args *parse_args(int argc, char *argv[], int *err_no)
{
    // TODO : check if -a present
    struct args *args = calloc(1, sizeof(struct args));
    args->config_path = NULL;

    if (argc == 2)
    {
        args->daemon = false;
        args->config_path = strdup(argv[1]);
        return args;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--dry-run") == 0)
            args->dry_run = true;
        else if (strcmp(argv[i], "-a") == 0)
        {
            args->daemon = true;
            i += 1;
            if (i >= argc)
            {
                free(args->config_path);
                free(args);
                *err_no = 1;
                return NULL;
            }
            if (strcmp(argv[i], "start") == 0)
                args->start = true;
            else if (strcmp(argv[i], "stop") == 0)
                args->stop = true;
            else if (strcmp(argv[i], "reload") == 0)
                args->reload = true;
            else if (strcmp(argv[i], "restart") == 0)
                args->restart = true;
            else
            {
                *err_no = 1;
                return args;
            }
        }
        else
            args->config_path = strdup(argv[i]);
    }

    return args;
}

void free_args(struct args *args)
{
    if (!args)
        return;
    free(args->config_path);
    free(args);
}

int dry_run(char *config_path)
{
    struct global *global = NULL;
    struct vhosts *vhosts = NULL;

    int err_no = 0;
    parse_config(&global, &vhosts, config_path, &err_no);
    check_config(global, vhosts, &err_no);
    if (err_no != 0)
    {
        free_global(global);
        free_vhosts(vhosts);
        errx(2, "./httpd --dry-run : Invalid Configuration file.");
    }
    free_global(global);
    free_vhosts(vhosts);

    return 0;
}

static bool process_alive(char *pid_file)
{
    FILE *fp = fopen(pid_file, "r");

    if (fp != NULL)
    {
        char *line = NULL;
        size_t len;

        getline(&line, &len, fp);
        pid_t pid = atol(line);
        fclose(fp);
        free(line);
        return kill(pid, 0) == 0;
    }

    return false;
}

int start_server(char *config_path, bool daemon)
{
    struct global *g = NULL;
    struct vhosts *v = NULL;

    int err_no = 0;
    int err_no2 = 0;
    parse_config(&g, &v, config_path, &err_no);

    if (g && v)
        check_config(g, v, &err_no2);

    if ((err_no | err_no2) != 0)
    {
        free_vhosts(v);
        free_global(g);
        return 2; // TODO : check subject to be sure
    }

    // if (!config_global_path)
    config_global_path = strdup(config_path);

    // basic launch (w/o daemon)
    if (!daemon)
    {
        launch(g, v);
        free_vhosts(v);
        free_global(g);
        return 0;
    }

    if (process_alive(g->pid_file))
    {
        free_vhosts(v);
        free_global(g);
        return 1;
    }

    FILE *fp = fopen(g->pid_file, "w+");
    pid_t val = fork();

    if (val == -1)
    {
        return 1;
    }
    int val_int = val;
    char *path = my_itoa(val_int);
    fputs(path, fp);
    free(path);

    if (!val)
    {
        if (g->log_file == NULL)
        {
            g->log_file = strdup("HTTPd.log");
        }

        launch(g, v);
    }

    free_vhosts(v);

    free_global(g);
    fclose(fp);

    return 0;
}

static int quit_server(char *config_path)
{
    struct global *g = NULL;
    struct vhosts *v = NULL;

    int err_no = 0;

    parse_config(&g, &v, config_path, &err_no);

    if (err_no != 0)
    {
        free_vhosts(v);
        free_global(g);
        return 2; // check subject to be sure
    }

    if (err_no == 0)
    {
        FILE *fp = fopen(g->pid_file, "r");
        if (fp != NULL)
        {
            char *line = NULL;
            size_t len;

            getline(&line, &len, fp);
            pid_t pid = atol(line);
            int alive = kill(pid, 0) == 0;
            if (alive)
            {
                kill(pid, SIGTERM);
            }

            remove(g->pid_file);

            free(line);
            fclose(fp);
        }
    }
    free_vhosts(v);
    free_global(g);

    return 0;
}

static int restart_server(char *config_path)
{
    int err = quit_server(config_path);
    if (err == 1)
    {
        printf("restart : ERROR\n");
        return err;
    }
    return start_server(config_path, true);
}

// TODO : check after
static int reload_server(char *config_path)
{
    struct global *g = NULL;
    struct vhosts *v = NULL;
    int err_no = 0;

    // check errors
    parse_config(&g, &v, config_path, &err_no);
    if (g == NULL || v == NULL)
    {
        printf("g or v NULL\n");
        return 2;
    }

    FILE *fp = fopen(g->pid_file, "r");
    char *line = NULL;
    size_t len;

    getline(&line, &len, fp);
    pid_t pid = atol(line);

    kill(pid, SIGUSR1);

    free(line);
    free_global(g);
    free_vhosts(v);

    return 0;
}

int main(int argc, char *argv[])
{
    if (!((argc == 3) || (argc == 4) || (argc == 2)))
    {
        return 1;
    }

    int err_no = 0;
    struct args *args = parse_args(argc, argv, &err_no);

    int err = 0;
    if (err_no != 0)
    {
        free_args(args);
        errx(1, "./httpd : Invalid Arguments.");
    }

    if (args->dry_run)
    {
        err = dry_run(args->config_path);
    }
    else if (args->start || (!args->daemon))
    {
        err = start_server(args->config_path, args->daemon);
    }
    else if (args->stop)
    {
        err = quit_server(args->config_path);
    }
    else if (args->restart)
    {
        err = restart_server(args->config_path);
    }
    else if (args->reload)
    {
        err = reload_server(args->config_path);
    }

    free_args(args);
    return err;
}
