CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -fsanitize=address

all: client server

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

clean:
	rm -f client server