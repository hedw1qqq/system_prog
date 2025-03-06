#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define _XOPEN_SOURCE 500

typedef enum statusCode {
    SUCCESS,
    WRONG_ARGS,
    WRONG_DIRECTORY,
    ERROR_PROCESS_FILE
} statusCode;

statusCode printFileInfo(const char *path, const char *filename) {
    struct stat fileStat;
    char fullPath[PATH_MAX];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", path, filename);

    if (stat(fullPath, &fileStat) == 0) {
        char timeStr[100];
        struct tm *tm_info = localtime(&fileStat.st_mtime);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);

        printf("File: %s, Inode: %lu, Created: %s, Type: ",
               filename, fileStat.st_ino, timeStr);

        if (S_ISREG(fileStat.st_mode)) {
            printf("Regular file\n");
        } else if (S_ISDIR(fileStat.st_mode)) {
            printf("Directory\n");
        } else if (S_ISLNK(fileStat.st_mode)) {
            printf("Symbolic link\n");
        } else if (S_ISFIFO(fileStat.st_mode)) {
            printf("FIFO/Named pipe\n");
        } else if (S_ISSOCK(fileStat.st_mode)) {
            printf("Socket\n");
        } else if (S_ISCHR(fileStat.st_mode)) {
            printf("Character device\n");
        } else if (S_ISBLK(fileStat.st_mode)) {
            printf("Block device\n");
        } else {
            printf("Other type\n");
        }
    } else {
        return ERROR_PROCESS_FILE;
    }
    return SUCCESS;
}

statusCode processDir(const char *pathDir) {
    struct dirent *dp;
    DIR *dir = opendir(pathDir);
    if (!dir) {
        fprintf(stderr, "Opening directory %s error \n", pathDir);
        return WRONG_DIRECTORY;
    }
    printf("Directory %s\n", pathDir);

    while ((dp = readdir(dir)) != NULL) {
        if (dp->d_name[0] == '.') {
            continue;
        }
        statusCode err = printFileInfo(pathDir, dp->d_name);
        if (err != SUCCESS) {
            closedir(dir);
            return err;
        }
    }
    closedir(dir);
    return SUCCESS;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "args must be >1\n");
        return WRONG_ARGS;
    }
    for (int i = 1; i < argc; i++) {
        char realPath[PATH_MAX];
        if (!realpath(argv[i], realPath)) {
            fprintf(stderr, "Error resolving path: %s\n", argv[i]);
            continue;
        }
        statusCode err = processDir(realPath);
        if (err != SUCCESS) {
            fprintf(stderr, "Error %d\n", err);
        }
    }
    return 0;
}