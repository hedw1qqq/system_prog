#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define SERVER_KEY_PATH "server.c"
#define SERVER_KEY_ID 1234
#define MAX_MSG_SIZE 512
#define SERVER_MSG_TYPE 1



typedef enum {
SUCCESS,
    OK_CONTINUE,
    OK_MOVED,
    OK_TOOK,
    OK_PUT,
    WIN,
    FAILURE,
    FAIL_WOLF_EATS_GOAT,
    FAIL_GOAT_EATS_CABBAGE,
    ERROR_ARGS,
    ERROR_BOAT_FULL,
    ERROR_NOTHING_TO_PUT,
    ERROR_WRONG_BANK,
    ERROR_UNKNOWN_OBJECT,
    ERROR_UNKNOWN_COMMAND,
    ERROR_SERVER_BUSY,
    ERROR_INVALID_USER_ID,
    ERROR_GAME_OVER,
    ERROR_INTERNAL
} StatusCode;

struct msgbuf {
    long mtype;
    char mtext[MAX_MSG_SIZE];
};

#endif // COMMON_H