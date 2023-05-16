CC=gcc
CPPFLAGS=-I./src
CFLAGS=-std=c99 -Wall -Wextra -Werror -pedantic -D_XOPEN_SOURCE=700 -g
LDFLAGS=-fsanitize=address
BIN=httpd
SRC=./src/main.c ./src/config_parser/config_parser.c ./src/tools/tools.c ./src/tools/list_tools.c ./src/request_handler/request_handler.c src/server/server.c src/tools/file_tools.c src/server/log.c

OBJS=$(SRC:.c=.o)

all: httpd

httpd: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(BIN)

clean:
	$(RM) $(BIN) $(OBJS)
