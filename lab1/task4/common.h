#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#define MAX_USERS 100


typedef enum statusCode {
    STATUS_SUCCESS = 0,
    STATUS_INVALID_COMMAND,
    STATUS_INVALID_OBJECT,
    STATUS_BOAT_FULL,
    STATUS_BOAT_EMPTY,
    STATUS_GAME_OVER,
    STATUS_VICTORY,
    STATUS_SERVER_ERROR,
    STATUS_AUTH_ERROR,
    STATUS_QUEUE_ERROR
} StatusCode;


typedef enum shore {
    SHORE_START = 0,
    SHORE_FINISH = 1,
    IN_BOAT = 2
} Shore;


typedef struct gameState {
    int userId;
    Shore farmerPosition;
    Shore wolfPosition;
    Shore goatPosition;
    Shore cabbagePosition;
    char itemInBoat[10];
    int moveCount;
    int gameOver;
} GameState;


typedef enum commandType {
    CMD_TAKE = 1,
    CMD_PUT = 2,
    CMD_MOVE = 3,
    CMD_NEW_GAME = 4,
    CMD_QUIT = 5
} CommandType;


typedef struct message {
    long messageType;
    int userId;
    CommandType command;
    char object[10];
    StatusCode status;
    GameState gameState;
} Message;


#endif
