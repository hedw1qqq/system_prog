#include "common.h"


statusCode process_path(const char *filepath, DirectoryContent *result) {
    char abs_filepath[PATH_MAX];
    struct stat path_stat;

    if (realpath(filepath, abs_filepath) == NULL) {
        result->status = PATH_ERROR;
        return PATH_ERROR;
    }


    if (stat(abs_filepath, &path_stat) != 0) {
        result->status = PATH_ERROR;
        return PATH_ERROR;
    }


    memset(result, 0, sizeof(DirectoryContent));
    result->status = SUCCESS;


    if (S_ISDIR(path_stat.st_mode)) {
        result->is_directory = 1;
        strcpy(result->path, abs_filepath);

        DIR *dir;
        struct dirent *entry;

        dir = opendir(abs_filepath);
        if (dir == NULL) {
            perror("Error opening directory");
            result->status = OPENDIR_ERROR;
            return OPENDIR_ERROR;
        }


        while ((entry = readdir(dir)) != NULL && result->file_count < 100) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                strcpy(result->files[result->file_count], entry->d_name);
                result->file_count++;
            }
        }

        closedir(dir);
    } else {
        result->is_directory = 0;
        strcpy(result->path, abs_filepath);
    }

    return SUCCESS;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[PATH_MAX];
    DirectoryContent result;


    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation error");
        return SOCKET_CREATE_ERROR;
    }


    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;


    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Socket binding error");
        close(server_socket);
        return BIND_ERROR;
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listening error");
        close(server_socket);
        return LISTEN_ERROR;
    }

    printf("Server started and waiting for connections...\n");

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Connection acceptance error");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            perror("Error receiving data");
            close(client_socket);
            continue;
        }

        statusCode status = process_path(buffer, &result);
        if (status != SUCCESS) {
            switch (status) {
                case PATH_ERROR:
                    fprintf(stderr, "Path error\n");
                    break;
                case OPENDIR_ERROR:
                    fprintf(stderr, "Open directory error\n");
                    break;
                default:
                    fprintf(stderr, "Unknown Error\n");
                    break;
            }
        }

        if (send(client_socket, &result, sizeof(result), 0) < 0) {
            perror("Error sending data");
        }

        close(client_socket);
    }

    close(server_socket);
    return SUCCESS;
}