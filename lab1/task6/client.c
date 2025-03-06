#include "common.h"

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    DirectoryContent result;
    char path[PATH_MAX];

    while (1) {
        printf("Введите абсолютный путь (или 'exit' для выхода): ");
        fgets(path, sizeof(path), stdin);

        path[strcspn(path, "\n")] = 0;

        if (strcmp(path, "exit") == 0) {
            break;
        }

        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket < 0) {
            perror("Ошибка создания сокета");
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            perror("Ошибка подключения к серверу");
            close(client_socket);
            continue;
        }

        if (send(client_socket, path, strlen(path), 0) < 0) {
            perror("Ошибка отправки данных");
            close(client_socket);
            continue;
        }

        if (recv(client_socket, &result, sizeof(result), 0) < 0) {
            perror("Ошибка получения данных");
            close(client_socket);
            continue;
        }

        switch (result.status) {
            case SUCCESS:
                if (result.is_directory) {
                    printf("Каталог: %s\n", result.path);
                    printf("Файлы:\n");
                    for (int i = 0; i < result.file_count; i++) {
                        printf("- %s\n", result.files[i]);
                    }
                } else {
                    printf("Абсолютный путь: %s\n", result.path);
                }
                break;
            case OPENDIR_ERROR:
                printf("Ошибка открытия директории: %s\n", path);
                break;
            case PATH_ERROR:
                printf("Ошибка: указанный путь не существует: %s\n", path);
                break;
            default:
                printf("Неизвестная ошибка при обработке пути\n");
        }

        close(client_socket);
    }

    return SUCCESS;
}