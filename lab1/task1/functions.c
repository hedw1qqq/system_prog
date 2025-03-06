#include "functions.h"

int validateUser(User *user) {
    if (!user) {
        return 0;
    }
    if (strlen(user->login) > 6 || user->PIN > 100000) {
        return 0;
    }
    for (int i = 0; i < strlen(user->login); i++) {
        if (!isalnum(user->login[i])) {
            return 0;
        }
    }
    return 1;
}

statusCode readUsersFromFile(const char *path, UserData *userArray) {
    if (!userArray) {
        return INVALID_ARGUMENT;
    }
    FILE *file = fopen(path, "r");
    if (!file) {

        userArray->data = malloc(sizeof(User) * userArray->capacity);
        if (!userArray->data) {
            return MEMORY_ALLOCATION_ERROR;
        }
        userArray->size = 0;
        return SUCCESS;
    }

    userArray->data = malloc(sizeof(User) * userArray->capacity);
    if (!userArray->data) {
        fclose(file);
        return MEMORY_ALLOCATION_ERROR;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        User user;
        user.commandsExecuted = 0;
        int read_count = sscanf(line, "%s %u %u", user.login, &user.PIN, &user.sanctions);
        if (read_count == 3 && validateUser(&user)) {
            if (userArray->size >= userArray->capacity) {
                size_t newCapacity = userArray->capacity * 2;
                User *temp = realloc(userArray->data, sizeof(User) * newCapacity);
                if (!temp) {
                    fclose(file);
                    free(userArray->data);
                    return MEMORY_ALLOCATION_ERROR;
                }
                userArray->data = temp;
                userArray->capacity = newCapacity;
            }
            userArray->data[userArray->size++] = user;
        }
    }

    fclose(file);
    return SUCCESS;
}

statusCode saveUsersToFile(const char *path, UserData *userArray) {
    if (!userArray || !userArray->data) {
        return INVALID_ARGUMENT;
    }

    FILE *file = fopen(path, "w");
    if (!file) {
        return INVALID_FILE;
    }

    for (size_t i = 0; i < userArray->size; i++) {
        fprintf(file, "%s %u %u\n",
                userArray->data[i].login,
                userArray->data[i].PIN,
                userArray->data[i].sanctions);
    }

    fclose(file);
    return SUCCESS;
}

int findUserByLogin(UserData *userArray, const char *login) {
    if (!userArray || !userArray->data || !login) {
        return -1;
    }

    for (size_t i = 0; i < userArray->size; i++) {
        if (strcmp(userArray->data[i].login, login) == 0) {
            return i;
        }
    }

    return -1;
}

statusCode registerUser(UserData *userArray) {
    if (!userArray || !userArray->data) {
        return INVALID_ARGUMENT;
    }

    User newUser;
    newUser.sanctions = 0;
    newUser.commandsExecuted = 0;

    printf("New user registration\n");
    printf("Enter login (max 6 characters, letters and digits only): ");
    scanf("%6s", newUser.login);


    for (size_t i = 0; i < strlen(newUser.login); i++) {
        if (!isalnum(newUser.login[i])) {
            printf("Invalid login. Use letters and digits only.\n");
            return AUTH_ERROR;
        }
    }

    if (findUserByLogin(userArray, newUser.login) != -1) {
        printf("User with this login already exists.\n");
        return AUTH_ERROR;
    }

    printf("Enter PIN (0 to 100000): ");
    if (scanf("%u", &newUser.PIN) != 1 || newUser.PIN > 100000) {
        printf("Invalid PIN.\n");

        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return AUTH_ERROR;
    }


    int c;
    while ((c = getchar()) != '\n' && c != EOF);


    if (userArray->size >= userArray->capacity) {
        size_t newCapacity = userArray->capacity * 2;
        User *temp = realloc(userArray->data, sizeof(User) * newCapacity);
        if (!temp) {
            return MEMORY_ALLOCATION_ERROR;
        }
        userArray->data = temp;
        userArray->capacity = newCapacity;
    }

    userArray->data[userArray->size++] = newUser;
    printf("User registered successfully.\n");

    return SUCCESS;
}

statusCode authenticateUser(UserData *userArray, int *currentUserIndex) {
    if (!userArray || !userArray->data || !currentUserIndex) {
        return INVALID_ARGUMENT;
    }

    char login[7];
    unsigned int pin;

    printf("Enter login: ");
    scanf("%6s", login);

    int userIndex = findUserByLogin(userArray, login);
    if (userIndex == -1) {
        printf("User not found.\n");
        return AUTH_ERROR;
    }

    printf("Enter PIN: ");
    if (scanf("%u", &pin) != 1) {
        printf("Invalid PIN.\n");

        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return AUTH_ERROR;
    }


    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    if (userArray->data[userIndex].PIN != pin) {
        printf("Incorrect PIN.\n");
        return AUTH_ERROR;
    }

    *currentUserIndex = userIndex;
    userArray->data[userIndex].commandsExecuted = 0;
    printf("Authentication successful. Welcome, %s!\n", login);

    return SUCCESS;
}

statusCode showTime() {
    time_t current_time;
    struct tm *timeinfo;
    char buffer[9];

    current_time = time(NULL);
    timeinfo = localtime(&current_time);
    if (!timeinfo) {
        return TIME_ERROR;
    }

    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    printf("Current time: %s\n", buffer);

    return SUCCESS;
}

statusCode showDate() {
    time_t current_time;
    struct tm *timeinfo;
    char buffer[11];

    current_time = time(NULL);
    timeinfo = localtime(&current_time);
    if (!timeinfo) {
        return TIME_ERROR;
    }

    strftime(buffer, sizeof(buffer), "%d:%m:%Y", timeinfo);
    printf("Current date: %s\n", buffer);

    return SUCCESS;
}


int parseDate(const char *dateStr, struct tm *timeinfo) {
    int day, month, year;
    if (sscanf(dateStr, "%d:%d:%d", &day, &month, &year) != 3) {
        return 0;
    }

    timeinfo->tm_mday = day;
    timeinfo->tm_mon = month - 1; // 0-11
    timeinfo->tm_year = year - 1900; // from 1900
    timeinfo->tm_hour = 0;
    timeinfo->tm_min = 0;
    timeinfo->tm_sec = 0;
//    timeinfo->tm_isdst = -1; // determine if daylight saving time is needed

    return 1;
}

statusCode howMuch(const char *flag, const char *dateStr) {
    if (!flag || !dateStr) {
        printf("Invalid command parameters.\n");
        return COMMAND_ERROR;
    }

    struct tm startTime;
    if (!parseDate(dateStr, &startTime)) {
        printf("Invalid date format. Use dd:mm:yyyy\n");
        return COMMAND_ERROR;
    }

    time_t startTimeT = mktime(&startTime);
    if (startTimeT == -1) {
        printf("Error converting date.\n");
        return TIME_ERROR;
    }

    time_t currentTime = time(NULL);
    double diff = difftime(currentTime, startTimeT);

    if (strcmp(flag, "-s") == 0) {
        printf("%.0f seconds have passed since the specified date.\n", diff);
    } else if (strcmp(flag, "-m") == 0) {
        printf("%.0f minutes have passed since the specified date.\n", diff / 60);
    } else if (strcmp(flag, "-h") == 0) {
        printf("%.0f hours have passed since the specified date.\n", diff / 3600);
    } else if (strcmp(flag, "-y") == 0) {
        printf("%.2f years have passed since the specified date.\n", diff / (365.25 * 24 * 3600));
    } else {
        printf("Unknown flag. Valid flags: -s, -m, -h, -y\n");
        return COMMAND_ERROR;
    }

    return SUCCESS;
}

statusCode applySanctions(UserData *userArray, const char *username, int currentUserIndex) {
    if (!userArray || !userArray->data || !username) {
        return INVALID_ARGUMENT;
    }

    int targetUserIndex = findUserByLogin(userArray, username);
    if (targetUserIndex == -1) {
        printf("User %s not found.\n", username);
        return COMMAND_ERROR;
    }


//    if (targetUserIndex == currentUserIndex) {
//        printf("You cannot apply sanctions to yourself.\n");
//        return COMMAND_ERROR;
//    }

    unsigned int confirmationCode;
    printf("Enter confirmation code (12345): ");
    if (scanf("%u", &confirmationCode) != 1 || confirmationCode != 12345) {
        printf("Invalid confirmation code.\n");

        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return COMMAND_ERROR;
    }

    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    unsigned int sanctionsCount;
    printf("Enter the maximum number of commands for user %s: ", username);
    if (scanf("%u", &sanctionsCount) != 1) {
        printf("Invalid value.\n");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return COMMAND_ERROR;
    }

    while ((c = getchar()) != '\n' && c != EOF);

    userArray->data[targetUserIndex].sanctions = sanctionsCount;
    printf("Sanctions successfully applied to user %s. Maximum commands: %u\n",
           username, sanctionsCount);

    return SUCCESS;
}

statusCode runShell(const char *filePath) {
    UserData userArray = {NULL, 10, 0};
    statusCode err = readUsersFromFile(filePath, &userArray);
    if (err != SUCCESS && err != INVALID_FILE) {
        printf("Error reading user file: %d\n", err);
        if (userArray.data) {
            free(userArray.data);
        }
        return err;
    }

    int currentUserIndex = -1;
    char command[256];
    int running = 1;

    // Main loop
    while (running) {
        if (currentUserIndex == -1) {

            printf("\n=== Authentication Menu ===\n");
            printf("1. Login\n");
            printf("2. Register\n");
            printf("3. Exit program\n");
            printf("Select an option: ");

            int choice;
            if (scanf("%d", &choice) != 1) {
                printf("Invalid input.\n");

                int c;
                while ((c = getchar()) != '\n' && c != EOF);
                continue;
            }


            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            switch (choice) {
                case 1:
                    authenticateUser(&userArray, &currentUserIndex);
                    break;
                case 2:
                    registerUser(&userArray);
                    saveUsersToFile(filePath, &userArray);
                    break;
                case 3:
                    running = 0;
                    break;
                default:
                    printf("Invalid choice.\n");
                    break;
            }
        } else {
            // проверка на санкции
            if (userArray.data[currentUserIndex].sanctions > 0 &&
                userArray.data[currentUserIndex].commandsExecuted >= userArray.data[currentUserIndex].sanctions) {
                printf("You have reached your command limit (%u). Logging out.\n",
                       userArray.data[currentUserIndex].sanctions);
                currentUserIndex = -1;
                continue;
            }


            printf("\n%s> ", userArray.data[currentUserIndex].login);
            if (!fgets(command, sizeof(command), stdin)) {
                continue;
            }


            command[strcspn(command, "\n")] = 0;


            char cmd[256];
            char arg1[256];
            char arg2[256];

            int args = sscanf(command, "%s %s %s", cmd, arg1, arg2);
            if (args < 1) {
                continue;
            }


            if (strcmp(cmd, "Time") == 0) {
                userArray.data[currentUserIndex].commandsExecuted++;
                showTime();
            }
            else if (strcmp(cmd, "Date") == 0) {
                userArray.data[currentUserIndex].commandsExecuted++;
                showDate();
            }
            else if (strcmp(cmd, "Howmuch") == 0 && args >= 3) {
                userArray.data[currentUserIndex].commandsExecuted++;
                howMuch(arg1, arg2);
            }
            else if (strcmp(cmd, "Logout") == 0) {
                printf("Logging out.\n");
                currentUserIndex = -1;
            }
            else if (strcmp(cmd, "Sanctions") == 0 && args >= 2) {
                userArray.data[currentUserIndex].commandsExecuted++;
                applySanctions(&userArray, arg1, currentUserIndex);
                saveUsersToFile(filePath, &userArray);
            }
            else if (strcmp(cmd, "Help") == 0) {
                printf("Available commands:\n");
                printf("  Time - Show current time\n");
                printf("  Date - Show current date\n");
                printf("  Howmuch [flag] [date] - Calculate time passed since date\n");
                printf("    Flags: -s (seconds), -m (minutes), -h (hours), -y (years)\n");
                printf("    Date format: dd:mm:yyyy\n");
                printf("  Sanctions [username] - Apply command limit to a user\n");
                printf("  Logout - Log out and return to authentication menu\n");
                printf("  Help - Show this help message\n");
            }
            else {
                printf("Unknown command. Type 'Help' for available commands.\n");
            }
        }
    }


    if (userArray.data) {
        free(userArray.data);
    }

    return SUCCESS;
}