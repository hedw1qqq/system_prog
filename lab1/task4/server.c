#include "common.h"

StatusCode checkGameState(GameState *game) {
    if (game->wolfPosition == SHORE_FINISH &&
        game->goatPosition == SHORE_FINISH &&
        game->cabbagePosition == SHORE_FINISH) {
        game->gameOver = 1;
        return STATUS_VICTORY;
    }

    if ((game->wolfPosition == game->goatPosition &&
         game->farmerPosition != game->wolfPosition &&
         game->wolfPosition != IN_BOAT && game->goatPosition != IN_BOAT) ||
        (game->goatPosition == game->cabbagePosition &&
         game->farmerPosition != game->goatPosition &&
         game->goatPosition != IN_BOAT && game->cabbagePosition != IN_BOAT)) {
        game->gameOver = 1;
        return STATUS_GAME_OVER;
    }

    return STATUS_SUCCESS;
}

StatusCode handleTakeCommand(GameState *game, const char *object) {
    if (strcmp(game->itemInBoat, "") != 0) {
        return STATUS_BOAT_FULL;
    }

    if (strcmp(object, "wolf") == 0) {
        if (game->wolfPosition == game->farmerPosition && game->wolfPosition != IN_BOAT) {
            game->wolfPosition = IN_BOAT;
            strcpy(game->itemInBoat, "wolf");
            return STATUS_SUCCESS;
        }
    } else if (strcmp(object, "goat") == 0) {
        if (game->goatPosition == game->farmerPosition && game->goatPosition != IN_BOAT) {
            game->goatPosition = IN_BOAT;
            strcpy(game->itemInBoat, "goat");
            return STATUS_SUCCESS;
        }
    } else if (strcmp(object, "cabbage") == 0) {
        if (game->cabbagePosition == game->farmerPosition && game->cabbagePosition != IN_BOAT) {
            game->cabbagePosition = IN_BOAT;
            strcpy(game->itemInBoat, "cabbage");
            return STATUS_SUCCESS;
        }
    } else {
        return STATUS_INVALID_OBJECT;
    }

    return STATUS_INVALID_OBJECT;
}

StatusCode handlePutCommand(GameState *game) {
    if (strcmp(game->itemInBoat, "") == 0) {
        return STATUS_BOAT_EMPTY;
    }

    if (strcmp(game->itemInBoat, "wolf") == 0) {
        game->wolfPosition = game->farmerPosition;
        strcpy(game->itemInBoat, "");
    } else if (strcmp(game->itemInBoat, "goat") == 0) {
        game->goatPosition = game->farmerPosition;
        strcpy(game->itemInBoat, "");
    } else if (strcmp(game->itemInBoat, "cabbage") == 0) {
        game->cabbagePosition = game->farmerPosition;
        strcpy(game->itemInBoat, "");
    }

    return STATUS_SUCCESS;
}

StatusCode handleMoveCommand(GameState *game) {
    game->farmerPosition = (game->farmerPosition == SHORE_START) ? SHORE_FINISH : SHORE_START;

    if (strcmp(game->itemInBoat, "wolf") == 0) {
        game->wolfPosition = IN_BOAT;
    } else if (strcmp(game->itemInBoat, "goat") == 0) {
        game->goatPosition = IN_BOAT;
    } else if (strcmp(game->itemInBoat, "cabbage") == 0) {
        game->cabbagePosition = IN_BOAT;
    }

    game->moveCount++;
    return STATUS_SUCCESS;
}

int main() {
    key_t key;
    int msgid;
    Message message;
    GameState games[MAX_USERS] = {0};
    int running = 1;
    int i;
    StatusCode status;

    key = ftok(".", 'a');
    if (key == -1) {
        perror("ftok error");
        return STATUS_QUEUE_ERROR;
    }

    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget error");
        return STATUS_QUEUE_ERROR;
    }

    printf("Server started. Waiting for messages...\n");

    while (running) {
        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), 1, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("msgrcv error");
            break;
        }

        i = 0;
        while (i < MAX_USERS && games[i].userId != 0 && games[i].userId != message.userId) {
            i++;
        }

        if (i == MAX_USERS) {
            message.status = STATUS_SERVER_ERROR;
            message.messageType = message.userId;
            msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
            continue;
        }

        switch (message.command) {
            case CMD_NEW_GAME:
                games[i].userId = message.userId;
                games[i].farmerPosition = SHORE_START;
                games[i].wolfPosition = SHORE_START;
                games[i].goatPosition = SHORE_START;
                games[i].cabbagePosition = SHORE_START;
                strcpy(games[i].itemInBoat, "");
                games[i].moveCount = 0;
                games[i].gameOver = 0;

                message.status = STATUS_SUCCESS;
                break;

            case CMD_TAKE:
                if (games[i].userId == 0) {
                    message.status = STATUS_AUTH_ERROR;
                } else {
                    message.status = handleTakeCommand(&games[i], message.object);
                }
                break;

            case CMD_PUT:
                if (games[i].userId == 0) {
                    message.status = STATUS_AUTH_ERROR;
                } else {
                    message.status = handlePutCommand(&games[i]);
                }
                break;

            case CMD_MOVE:
                if (games[i].userId == 0) {
                    message.status = STATUS_AUTH_ERROR;
                } else {
                    message.status = handleMoveCommand(&games[i]);
                }
                break;

            case CMD_QUIT:
                games[i].userId = 0;
                message.status = STATUS_SUCCESS;
                break;

            default:
                message.status = STATUS_INVALID_COMMAND;
                break;
        }

        if (message.status == STATUS_SUCCESS && message.command != CMD_QUIT) {
            status = checkGameState(&games[i]);
            if (status != STATUS_SUCCESS) {
                message.status = status;
            }
        }

        message.messageType = message.userId;
        memcpy(&message.gameState, &games[i], sizeof(GameState));

        if (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("msgsnd error");
        }
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl error");
    }

    return STATUS_SUCCESS;
}