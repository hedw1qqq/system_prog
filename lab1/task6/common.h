//
// Created by ivglu on 04.03.2025.
//

#ifndef SYST_PROG_COMMON_H
#define SYST_PROG_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#define PORT 8080



typedef enum {
    SUCCESS,
    SOCKET_CREATE_ERROR,
    BIND_ERROR,
    LISTEN_ERROR,
    ACCEPT_ERROR,
    RECV_ERROR,
    SEND_ERROR,
    OPENDIR_ERROR,
    PATH_ERROR
} statusCode;

typedef struct {
    char path[PATH_MAX];
    char files[100][256];
    int file_count;
    int is_directory;
    statusCode status;
} DirectoryContent;
#endif //SYST_PROG_COMMON_H
