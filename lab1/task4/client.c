#include "common.h"

void printGameState(const GameState *state) {
    printf("Current state:\n");
    printf("  Farmer: %s\n", state->farmerPosition == SHORE_START ? "Starting shore" : "Finishing shore");
    printf("  Wolf: %s\n",
           state->wolfPosition == SHORE_START ? "Starting shore" :
           (state->wolfPosition == SHORE_FINISH ? "Finishing shore" : "In boat"));
    printf("  Goat: %s\n",
           state->goatPosition == SHORE_START ? "Starting shore" :
           (state->goatPosition == SHORE_FINISH ? "Finishing shore" : "In boat"));
    printf("  Cabbage: %s\n",
           state->cabbagePosition == SHORE_START ? "Starting shore" :
           (state->cabbagePosition == SHORE_FINISH ? "Finishing shore" : "In boat"));
    printf("  Item in boat: %s\n", state->itemInBoat);
    printf("  Moves: %d\n", state->moveCount);
    printf("\n");
}

void trimString(char *str) {
    size_t len = strlen(str);

    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

int main(int argc, char *argv[]) {
    key_t key;
    int msgid;
    Message message;
    FILE *fp;
    char line[256];
    int userId;


    if (argc < 2) {
        fprintf(stderr, "Usage: %s <commands_file>\n", argv[0]);
        return STATUS_INVALID_COMMAND;
    }

    srand(time(NULL) + getpid());
    userId = rand() % 10000 + 1;
    printf("Client started with user ID: %d\n", userId);

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

    message.messageType = 1;
    message.userId = userId;
    message.command = CMD_NEW_GAME;
    strcpy(message.object, "");

    if (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("msgsnd error");
        return STATUS_QUEUE_ERROR;
    }

    if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), userId, 0) == -1) {
        perror("msgrcv error");
        return STATUS_QUEUE_ERROR;
    }

    if (message.status != STATUS_SUCCESS) {
        fprintf(stderr, "Error starting new game: status code %d\n", message.status);
        return message.status;
    }

    printf("New game started successfully.\n");
    printGameState(&message.gameState);

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Error opening file");
        return STATUS_SERVER_ERROR;
    }

    while (fgets(line, sizeof(line), fp)) {
        trimString(line);

        if (strncmp(line, "take ", 5) == 0) {
            message.command = CMD_TAKE;
            char *object = line + 5;
            size_t objLen = strlen(object);

            if (objLen > 0 && object[objLen - 1] == ';') {
                object[objLen - 1] = '\0';
            }

            strcpy(message.object, object);
        } else if (strcmp(line, "put;") == 0) {
            message.command = CMD_PUT;
            strcpy(message.object, "");
        } else if (strcmp(line, "move;") == 0) {
            message.command = CMD_MOVE;
            strcpy(message.object, "");
        } else {
            printf("Invalid command: %s\n", line);
            continue;
        }

        message.messageType = 1;
        message.userId = userId;

        if (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("msgsnd error");
            break;
        }

        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), userId, 0) == -1) {
            perror("msgrcv error");
            break;
        }

        switch (message.status) {
            case STATUS_SUCCESS:
                printf("Command '%s' executed successfully.\n", line);
                break;
            case STATUS_INVALID_COMMAND:
                printf("Invalid command: %s\n", line);
                break;
            case STATUS_INVALID_OBJECT:
                printf("Invalid object specified in: %s\n", line);
                break;
            case STATUS_BOAT_FULL:
                printf("Boat is already full: %s\n", line);
                break;
            case STATUS_BOAT_EMPTY:
                printf("Nothing to put from the boat: %s\n", line);
                break;
            case STATUS_GAME_OVER:
                printf("Game over: %s\n",
                    (message.gameState.wolfPosition == message.gameState.goatPosition &&
                     message.gameState.wolfPosition != IN_BOAT &&
                     message.gameState.farmerPosition != message.gameState.wolfPosition) ?
                     "Wolf ate the goat!" : "Goat ate the cabbage!");
                message.command = CMD_QUIT;
                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                fclose(fp);
                return STATUS_GAME_OVER;
            case STATUS_VICTORY:
                printf("Victory! All items successfully transported in %d moves.\n", message.gameState.moveCount);
                message.command = CMD_QUIT;
                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                fclose(fp);
                return STATUS_VICTORY;
            default:
                fprintf(stderr, "Error processing command: status code %d\n", message.status);
                break;
        }

        printGameState(&message.gameState);
    }

    fclose(fp);

    message.messageType = 1;
    message.userId = userId;
    message.command = CMD_QUIT;
    strcpy(message.object, "");

    if (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("msgsnd error");
    }

    return STATUS_SUCCESS;
}