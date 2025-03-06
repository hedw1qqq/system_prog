#include "functions.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <user_file_path>\n", argv[0]);
        return INVALID_ARGS;
    }

    const char *path = argv[1];
    return runShell(path);
}