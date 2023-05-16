#include "log.h"

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

char *get_date(void)
{
    size_t max_size = 100;
    char *date = malloc(max_size * sizeof(char));
    time_t rawtime;
    struct tm *info;

    time(&rawtime);

    info = gmtime(&rawtime);

    strftime(date, max_size, "%a, %d %b %Y %T GMT", info); // removed Date:

    return date;
}
