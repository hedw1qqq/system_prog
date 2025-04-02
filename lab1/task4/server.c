#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <limits.h>
#include "common.h"

#define MAX_CLIENTS 10

#define WOLF 0
#define GOAT 1
#define CABBAGE 2
#define NOTHING -1

#define LEFT_BANK 0
#define RIGHT_BANK 1

#define NO_LOSS 0
#define WOLF_GOAT_LOSS 1
#define GOAT_CABBAGE_LOSS 2


typedef struct {
    long user_id;
    int items[3];
    int farmer_pos;
    int boat_load;
    int active;
    int game_over;
} GameState;

GameState client_states[MAX_CLIENTS];
int msqid = -1;

void initialize_states() {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        client_states[i].active = 0;
        client_states[i].user_id = -1;
        client_states[i].game_over = 0;
    }
}

GameState *find_or_create_state(long user_id) {
    GameState *free_slot = NULL;
    if (user_id <= 0) return NULL;

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_states[i].active && client_states[i].user_id == user_id) {
            return &client_states[i];
        }
        if (!client_states[i].active && free_slot == NULL) {
            free_slot = &client_states[i];
        }
    }

    if (free_slot != NULL) {
        free_slot->active = 1;
        free_slot->user_id = user_id;
        free_slot->items[WOLF] = LEFT_BANK;
        free_slot->items[GOAT] = LEFT_BANK;
        free_slot->items[CABBAGE] = LEFT_BANK;
        free_slot->farmer_pos = LEFT_BANK;
        free_slot->boat_load = NOTHING;
        free_slot->game_over = 0;
        return free_slot;
    }

    return NULL;
}

void deactivate_state(GameState *state) {
    if (state) {
        state->active = 0;
        state->user_id = -1;
        state->game_over = 0;
    }
}


int check_loss_condition(const GameState *state) {
    int unattended_bank = 1 - state->farmer_pos;

    int wolf_on_unattended = (state->items[WOLF] == unattended_bank);
    int goat_on_unattended = (state->items[GOAT] == unattended_bank);
    int cabbage_on_unattended = (state->items[CABBAGE] == unattended_bank);

    if (wolf_on_unattended && goat_on_unattended) {
        return WOLF_GOAT_LOSS;
    }
    if (goat_on_unattended && cabbage_on_unattended) {
        return GOAT_CABBAGE_LOSS;
    }

    return NO_LOSS;
}


int check_win_condition(const GameState *state) {
    return state->items[WOLF] == RIGHT_BANK &&
           state->items[GOAT] == RIGHT_BANK &&
           state->items[CABBAGE] == RIGHT_BANK &&
           state->farmer_pos == RIGHT_BANK;
}

StatusCode process_take(GameState *state, const char *arg) {
    if (!arg || strlen(arg) == 0) return ERROR_ARGS;
    if (state->boat_load != NOTHING) return ERROR_BOAT_FULL;

    int item_to_take = NOTHING;
    if (strcmp(arg, "wolf") == 0) {
        item_to_take = WOLF;
    } else if (
        strcmp(arg, "goat") == 0) {
        item_to_take = GOAT;
    } else if (strcmp(arg, "cabbage") == 0) {
        item_to_take = CABBAGE;
    } else {
        return ERROR_UNKNOWN_OBJECT;
    }

    if (state->items[item_to_take] != state->farmer_pos) return ERROR_WRONG_BANK;

    state->boat_load = item_to_take;
    state->items[item_to_take] = -1;
    return OK_TOOK;
}

StatusCode process_put(GameState *state) {
    if (state->boat_load == NOTHING) return ERROR_NOTHING_TO_PUT;
    state->items[state->boat_load] = state->farmer_pos;
    state->boat_load = NOTHING;

    int loss_type = check_loss_condition(state);
    if (loss_type != NO_LOSS) {
        state->game_over = 1;
        return (loss_type == WOLF_GOAT_LOSS) ? FAIL_WOLF_EATS_GOAT : FAIL_GOAT_EATS_CABBAGE;
    }
    if (check_win_condition(state)) {
        state->game_over = 1;
        return WIN;
    }

    return OK_PUT;
}


StatusCode process_move(GameState *state) {
    state->farmer_pos = 1 - state->farmer_pos;

    int loss_type = check_loss_condition(state);
    if (loss_type != NO_LOSS) {
        state->game_over = 1;
        return (loss_type == WOLF_GOAT_LOSS) ? FAIL_WOLF_EATS_GOAT : FAIL_GOAT_EATS_CABBAGE;
    }
    if (check_win_condition(state)) {
        state->game_over = 1;
        return WIN;
    }

    return OK_MOVED;
}


StatusCode process_command(GameState *state, const char *command) {
    if (state->game_over) return ERROR_GAME_OVER;

    char cmd[20];
    char arg[20];
    char extra[2] = "";
    int parsed = sscanf(command, "%19s %19s %1s", cmd, arg, extra);

    if (strcmp(cmd, "take") == 0) {
        if (parsed != 2) return ERROR_ARGS;

        return process_take(state, arg);
    } else if (strcmp(cmd, "put") == 0) {
        if (parsed != 1) return ERROR_ARGS;
        return process_put(state);
    } else if (strcmp(cmd, "move") == 0) {
        if (parsed != 1) return ERROR_ARGS;
        return process_move(state);
    } else {
        return ERROR_UNKNOWN_COMMAND;
    }
}

void send_response(long user_id, StatusCode code, const char *message) {
    struct msgbuf response_msg;
    response_msg.mtype = user_id;

    snprintf(response_msg.mtext, MAX_MSG_SIZE, "%d %s", code, message ? message : "");
    response_msg.mtext[MAX_MSG_SIZE - 1] = '\0';


    if (msgsnd(msqid, &response_msg, strlen(response_msg.mtext) + 1, 0) == -1) {
        GameState *state = find_or_create_state(user_id);
        if (state && state->active) {
            deactivate_state(state);
        }
    }
}

const char *get_status_message(StatusCode code, const GameState *state) {
    switch (code) {
        case OK_CONTINUE:
            return "Command accepted.";
        case OK_MOVED:
            return state
                       ? (state->farmer_pos == LEFT_BANK ? "Moved to left bank." : "Moved to right bank.")
                       : "Moved.";
        case OK_TOOK:
            if (!state || state->boat_load == NOTHING) return "Took item.";
            return (state->boat_load == WOLF)
                       ? "Took wolf."
                       : (state->boat_load == GOAT)
                             ? "Took goat."
                             : "Took cabbage.";
        case OK_PUT:

            return state
                       ? (state->farmer_pos == LEFT_BANK ? "Put item on left bank." : "Put item on right bank.")
                       : "Put item.";
        case WIN:
            return "WIN: All items successfully transported!";
        case FAIL_WOLF_EATS_GOAT:
            return "FAIL: Wolf ate the goat!";
        case FAIL_GOAT_EATS_CABBAGE:
            return "FAIL: Goat ate the cabbage!";
        case ERROR_ARGS:
            return "Error: Invalid arguments or format for command.";
        case ERROR_BOAT_FULL:
            return "Error: Boat is already full.";
        case ERROR_NOTHING_TO_PUT:
            return "Error: Boat is empty, nothing to put.";
        case ERROR_WRONG_BANK:
            return "Error: Object is not on this bank.";
        case ERROR_UNKNOWN_OBJECT:
            return "Error: Unknown object specified.";
        case ERROR_UNKNOWN_COMMAND:
            return "Error: Unknown command.";
        case ERROR_SERVER_BUSY:
            return "Error: Server is busy, max clients reached.";
        case ERROR_INVALID_USER_ID:
            return "Error: Invalid User ID received.";
        case ERROR_GAME_OVER:
            return "Error: Game already finished (Win/Fail). Cannot accept more commands.";
        case ERROR_INTERNAL:
            return "Error: Internal server error.";
        default:
            return "Unknown status code.";
    }
}

void cleanup(int signum) {
    (void) signum;
    if (msqid != -1) {
        msgctl(msqid, IPC_RMID, NULL);
    }
    _exit(SUCCESS);
}

int main() {
    struct msgbuf msg;
    struct sigaction sa;

    sa.sa_handler = cleanup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, "Server Error: Failed to set signal handlers.\n");
        return FAILURE;
    }

    initialize_states();

    key_t key = ftok(SERVER_KEY_PATH, SERVER_KEY_ID);
    if (key == -1) {
        fprintf(stderr, "Server Error: ftok failed. Ensure '%s' exists.\n", SERVER_KEY_PATH);
        return FAILURE;
    }

    msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        fprintf(stderr, "Server Error: msgget failed. Check permissions or existing queue.\n");
        return FAILURE;
    }

    while (1) {
        if (msgrcv(msqid, &msg, sizeof(msg.mtext), SERVER_MSG_TYPE, 0) == -1) {
            break;
        }

        long user_id;
        char client_command[MAX_MSG_SIZE];
        char *endptr;
        char *first_space = strchr(msg.mtext, ' ');

        if (!first_space) {
            fprintf(stderr, "Server Warning: Received malformed message (no space after ID): %s\n", msg.mtext);
            continue;
        }

        *first_space = '\0';
        user_id = strtol(msg.mtext, &endptr, 10);

        if (*endptr != '\0' || user_id <= 0 || user_id > LONG_MAX) {
            *first_space = ' ';
            fprintf(stderr, "Server Warning: Received invalid User ID format or value: %s\n", msg.mtext);
            continue;
        }

        *first_space = ' ';
        strncpy(client_command, first_space + 1, sizeof(client_command) - 1);
        client_command[sizeof(client_command) - 1] = '\0';

        GameState *current_state = find_or_create_state(user_id);

        StatusCode response_code;
        const char *response_msg_text;

        if (current_state == NULL) {
            response_code = ERROR_SERVER_BUSY;
            response_msg_text = get_status_message(response_code, NULL);
            send_response(user_id, response_code, response_msg_text);
        } else {
            response_code = process_command(current_state, client_command);
            response_msg_text = get_status_message(response_code, current_state);
            send_response(user_id, response_code, response_msg_text);


            if (response_code == WIN || response_code == FAIL_WOLF_EATS_GOAT || response_code == FAIL_GOAT_EATS_CABBAGE
                || response_code == ERROR_GAME_OVER) {
                if (current_state->active) {
                    deactivate_state(current_state);
                }
            }
        }
    }

    cleanup(0);
    return SUCCESS;
}
