#include "file_tools.h"

// TODO : recheck
bool file_is_readable(const char *file)
{
    struct stat s;
    stat(file, &s);
    return s.st_mode & S_IRUSR;
}

bool file_exists(const char *file)
{
    struct stat s;
    stat(file, &s);
    return S_ISREG(s.st_mode);
}

bool is_dir(const char *path)
{
    struct stat s;
    return stat(path, &s) == 0;
}

ssize_t get_file_len(char *path)
{
    FILE *fp = fopen(path, "r");

    if (fp == NULL)
    {
        return -1;
    }

    fseek(fp, 0, SEEK_END);

    ssize_t res = ftell(fp);
    fclose(fp);
    return res;
}

void close_fd(int cfd)
{
    shutdown(cfd, SHUT_RDWR);
    close(cfd);
}
