#ifndef SYST_PROG_FUNCTIONS_H
#define SYST_PROG_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

typedef enum statusCode {
    SUCCESS,
    INVALID_ARGS,
    INVALID_FILE,
    MEMORY_ALLOCATION_ERROR,
    INVALID_ARGUMENT,
    TIME_ERROR,
    AUTH_ERROR,
    COMMAND_ERROR
} statusCode;

typedef struct User {
    char login[7];
    unsigned int PIN;
    unsigned int sanctions;
    unsigned int commandsExecuted;
} User;

typedef struct UserData {
    User *data;
    size_t capacity;
    size_t size;
} UserData;


int validateUser(User *user);
statusCode readUsersFromFile(const char *path, UserData *userArray);
statusCode saveUsersToFile(const char *path, UserData *userArray);
int findUserByLogin(UserData *userArray, const char *login);
statusCode registerUser(UserData *userArray);


statusCode showTime();
statusCode showDate();
statusCode howMuch(const char *flag, const char *date);
statusCode applySanctions(UserData *userArray, const char *username, int currentUserIndex);


statusCode runShell(const char *filePath);
statusCode authenticateUser(UserData *userArray, int *currentUserIndex);

#endif //SYST_PROG_FUNCTIONS_H