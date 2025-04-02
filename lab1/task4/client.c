#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <limits.h>
#include "common.h"

const char* get_client_status_message(StatusCode code, const char* details) {

     if (details && strlen(details) > 0) {
         return details;
     }

     switch(code) {
        case OK_PUT:
             return "OK";
        case WIN:
            return "WIN!";
        case FAIL_WOLF_EATS_GOAT:
            return "FAIL: Wolf ate the goat!";
        case FAIL_GOAT_EATS_CABBAGE:
            return "FAIL: Goat ate the cabbage!";
        case ERROR_INTERNAL:
             return "Error received from server.";
        default:
             return "Received unknown status code from server.";
     }
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <user_id> <command_file>\n", argv[0]);
        return FAILURE;
    }

    char *endptr;

    long long id_temp = strtoll(argv[1], &endptr, 10);


    if (*endptr != '\0' || id_temp <= 0 || id_temp > LONG_MAX) {
         fprintf(stderr, "Client Error: Invalid user_id '%s'. user_id must be > 0.\n", argv[1]);
         return FAILURE;
    }
    long user_id = (long) id_temp;

    const char *filename = argv[2];
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Client Error: Could not open command file '%s'.\n", filename);
        return FAILURE;
    }

    key_t key = ftok(SERVER_KEY_PATH, SERVER_KEY_ID);
     if (key == -1) {
        fprintf(stderr, "Client Error: ftok failed.\n");
        fclose(file);
        return FAILURE;
    }

    int msqid = msgget(key, 0666);
    if (msqid == -1) {
        fprintf(stderr, "Client Error: Failed to get message queue\n");
        fclose(file);
        return FAILURE;
    }

    struct msgbuf send_buf;
    struct msgbuf recv_buf;
    char command_line[MAX_MSG_SIZE / 2];

    int game_finished = 0;

    while (!game_finished && fgets(command_line, sizeof(command_line), file)) {
        command_line[strcspn(command_line, "\n")] = 0;

        if (strlen(command_line) == 0) {
            continue;
        }

        send_buf.mtype = SERVER_MSG_TYPE;
        snprintf(send_buf.mtext, MAX_MSG_SIZE, "%ld %s", user_id, command_line);
        send_buf.mtext[MAX_MSG_SIZE - 1] = '\0';

        if (msgsnd(msqid, &send_buf, strlen(send_buf.mtext) + 1, 0) == -1) {
            fprintf(stderr, "Client Error: Failed to send message to server.\n");
            break;
        }

        if (msgrcv(msqid, &recv_buf, sizeof(recv_buf.mtext), user_id, 0) == -1) {
             fprintf(stderr, "Client Error: Failed to receive message from server or queue removed.\n");
            break;
        }

        int received_status_code_int;
        char response_details[MAX_MSG_SIZE];
        response_details[0] = '\0';

        int parsed_items = sscanf(recv_buf.mtext, "%d %[^\n]", &received_status_code_int, response_details);

        if (parsed_items < 1) {
             fprintf(stderr, "Client Error: Received malformed response from server: %s\n", recv_buf.mtext);
             break;
        }

        if (received_status_code_int < OK_CONTINUE || received_status_code_int > ERROR_INTERNAL) {
             fprintf(stderr, "Client Error: Received invalid status code from server: %d\n", received_status_code_int);
             game_finished = 1;
             continue;
        }


        StatusCode received_status = (StatusCode)received_status_code_int;
        const char* display_message = get_client_status_message(received_status, response_details);

        printf("Client: Server response: %s\n", display_message);


        if (received_status == WIN || received_status == FAIL_WOLF_EATS_GOAT || received_status == FAIL_GOAT_EATS_CABBAGE || received_status == ERROR_GAME_OVER) {
            game_finished = 1;
        } else if (received_status >= ERROR_ARGS && received_status <= ERROR_INTERNAL) {

             game_finished = 1;
        }
    }

    fclose(file);
    return SUCCESS;
}