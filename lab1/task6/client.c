#include "common.h"

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    DirectoryContent result;
    char path[PATH_MAX];

    while (1) {
        printf("Enter absolute path (or 'exit' to quit): ");
        fgets(path, sizeof(path), stdin);

        path[strcspn(path, "\n")] = 0;

        if (strcmp(path, "exit") == 0) {
            break;
        }

        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket < 0) {
            perror("Socket creation error");
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            perror("Connection error to server");
            close(client_socket);
            continue;
        }

        if (send(client_socket, path, strlen(path), 0) < 0) {
            perror("Error sending data");
            close(client_socket);
            continue;
        }

        if (recv(client_socket, &result, sizeof(result), 0) < 0) {
            perror("Error receiving data");
            close(client_socket);
            continue;
        }

        switch (result.status) {
            case SUCCESS:
                if (result.is_directory) {
                    printf("Directory: %s\n", result.path);
                    printf("Files:\n");
                    for (int i = 0; i < result.file_count; i++) {
                        printf("- %s\n", result.files[i]);
                    }
                } else {
                    printf("Absolute path: %s\n", result.path);
                }
                break;
            case OPENDIR_ERROR:
                printf("Error opening directory: %s\n", path);
                break;
            case PATH_ERROR:
                printf("Error: specified path does not exist: %s\n", path);
                break;
            default:
                printf("Unknown error while processing path\n");
        }

        close(client_socket);
    }

    return SUCCESS;
}