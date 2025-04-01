#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
typedef enum StatusCode {
    SUCCESS,
    ERROR_FILE_OPEN,
    ERROR_MEMORY,
    ERROR_FORK,
    ERROR_READ,
    ERROR_WRITE,
    ERROR_INVALID_ARGS,
} StatusCode;

StatusCode processXorN(const char *filepath, int n, uint64_t *result) {
    if (!filepath || !result) {
        return ERROR_INVALID_ARGS;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return ERROR_FILE_OPEN;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    size_t total_bits = file_size * 8;
    size_t block_bits = 1 << n;
    size_t block_bytes = block_bits / 8;
    size_t count_blocks = (total_bits + block_bits - 1) / block_bits;

    size_t val = 0;
    size_t tmp_val;
    unsigned char buf[block_bytes];

    for (size_t i = 0; i < count_blocks; ++i) {
        tmp_val = 0;
        size_t real_count_bytes = fread(buf, 1, block_bytes, file);
        if (real_count_bytes < block_bytes) {
            memset(buf + real_count_bytes, 0, block_bytes - real_count_bytes);
        }
        for (int current_bit = 0; current_bit < block_bits; ++current_bit) {
            size_t byte_idx = current_bit / 8;
            size_t bit_idx = 7 - (current_bit % 8);
            int bit = (buf[byte_idx] >> bit_idx) & 1;
            tmp_val = (tmp_val << 1) | bit;
        }
        val ^= tmp_val;
    }

    fclose(file);
    *result = val;
    return SUCCESS;
}

StatusCode processMask(const char *filepath, const char *mask_string, int *res) {
    if (!filepath || !res || !mask_string) {
        return ERROR_INVALID_ARGS;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return ERROR_FILE_OPEN;
    }

    uint32_t mask;
    if (sscanf(mask_string, "%x", &mask) != 1) {
        fclose(file);
        return ERROR_INVALID_ARGS;
    }

    uint32_t val;
    *res = 0;
    while (fread(&val, sizeof(val), 1, file) == 1) {
        if (val == mask) {
            (*res)++;
        }
    }

    fclose(file);
    return SUCCESS;
}

StatusCode str_to_int(const char *str, int *res) {
    if (!str || !res) {
        return ERROR_INVALID_ARGS;
    }

    char *endptr;
    long tmp = strtol(str, &endptr, 10);

    if (*endptr != '\0') {
        return ERROR_INVALID_ARGS;
    }

    if (tmp < 0 || tmp > INT_MAX) {
        return ERROR_INVALID_ARGS;
    }

    *res = (int) tmp;
    return SUCCESS;
}

StatusCode processCopy(const char *filepath, int n) {
    if (!filepath || n <= 0) {
        return ERROR_INVALID_ARGS;
    }

    for (int i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            return ERROR_FORK;
        }
        if (pid == 0) {
            FILE *file = fopen(filepath, "rb");
            if (!file) {
                exit(ERROR_FILE_OPEN);
            }

            char new_filename[256];
            snprintf(new_filename, sizeof(new_filename), "%s_%d", filepath, i+1);
            FILE *new_file = fopen(new_filename, "wb");
            if (!new_file) {
                fclose(file);
                exit(ERROR_FILE_OPEN);
            }

            int ch;
            while ((ch = fgetc(file)) != EOF) {
                fputc(ch, new_file);
            }

            fclose(file);
            fclose(new_file);
            exit(EXIT_SUCCESS);
        }
    }
    for (int i = 0; i < n; ++i) {
        wait(NULL);
    }

    return SUCCESS;
}
char* parse_escape_sequences(const char* input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;

    size_t i = 0, j = 0;
    while (i < len) {
        if (input[i] == '\\' && i + 1 < len) {
            switch (input[i+1]) {
                case 'n':
                    result[j++] = '\n';
                i += 2;
                break;
                case 't':
                    result[j++] = '\t';
                i += 2;
                break;
                case 'r':
                    result[j++] = '\r';
                i += 2;
                break;
                case '\\':
                    result[j++] = '\\';
                i += 2;
                break;
                default:
                    result[j++] = input[i++];
                break;
            }
        } else {
            result[j++] = input[i++];
        }
    }
    result[j] = '\0';
    return result;
}

StatusCode processFind(const char *filepath, const char* str, int *res) {
    if (!filepath || !str || !res) {
        return ERROR_INVALID_ARGS;
    }

    char* parsed_str = parse_escape_sequences(str);
    if (!parsed_str) {
        return ERROR_MEMORY;
    }

    pid_t pid = fork();

    if (pid < 0) {
        free(parsed_str);
        return ERROR_FORK;
    }

    if (pid == 0) {

        FILE *file = fopen(filepath, "r");
        if (!file) {
            free(parsed_str);
            exit(ERROR_FILE_OPEN);
        }

        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        rewind(file);

        char *buffer = (char*)malloc(file_size + 1);
        if (!buffer) {
            fclose(file);
            free(parsed_str);
            exit(ERROR_MEMORY);
        }

        size_t bytes_read = fread(buffer, 1, file_size, file);
        buffer[bytes_read] = '\0';

        fclose(file);

        int found = (strstr(buffer, parsed_str) != NULL);

        free(buffer);
        free(parsed_str);
        exit(found ? EXIT_SUCCESS : EXIT_FAILURE);
    } else {

        free(parsed_str);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            *res = (WEXITSTATUS(status) == EXIT_SUCCESS) ? 1 : 0;
            return SUCCESS;
        } else {
            return ERROR_READ;
        }
    }
}


int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file1> [file2 ...] <flag> [options]\n", argv[0]);
        return ERROR_INVALID_ARGS;
    }

    int flag_index = -1;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "xor", 3) == 0 || strncmp(argv[i], "mask", 4) == 0 || strncmp(argv[i], "copy", 4) == 0 || strncmp(argv[i], "find", 4) == 0) {
            flag_index = i;
            break;
        }
    }

    if (flag_index == -1 || flag_index == 1) {
        fprintf(stderr, "Invalid arguments\n");
        return ERROR_INVALID_ARGS;
    }

    char *flag = argv[flag_index];
    int file_count = flag_index - 1;

    if (strncmp(flag, "xor", 3) == 0) {
        int n = atoi(flag + 3);
        if (n < 2 || n > 6) {
            fprintf(stderr, "Invalid N\n");
            return ERROR_INVALID_ARGS;
        }

        StatusCode status;
        for (int i = 1; i < file_count + 1; i++) {
            uint64_t result = 0;
            status = processXorN(argv[i], n, &result);
            if (status != SUCCESS) {
                fprintf(stderr, "Error in file %s\n", argv[i]);
                continue;
            }
            printf("xorN result for file %s = %lu\n", argv[i], result);
        }
    } else if (strncmp(flag, "mask", 4) == 0) {
        if (flag_index + 1 >= argc) {
            fprintf(stderr, "Missing mask value\n");
            return ERROR_INVALID_ARGS;
        }
        const char *mask_string = argv[flag_index + 1];
        StatusCode status;
        for (int i = 1; i < file_count + 1; i++) {
            int result = 0;
            status = processMask(argv[i], mask_string, &result);
            if (status != SUCCESS) {
                fprintf(stderr, "Error in file %s\n", argv[i]);
                continue;
            }
            printf("mask result for file %s = %d\n", argv[i], result);
        }
    } else if (strncmp(flag, "copy", 4) == 0) {
        int n = 0;

        StatusCode status = str_to_int(flag + 4, &n);
        if (status != SUCCESS) {
            fprintf(stderr, "N must be int\n");
            return ERROR_INVALID_ARGS;
        }
        if (n < 0) {
            fprintf(stderr, "Invalid N\n");
            return ERROR_INVALID_ARGS;
        }
        for (int i = 1; i < file_count + 1; i++) {
            StatusCode status = processCopy(argv[i], n);
            if (status != SUCCESS) {
                fprintf(stderr, "Error in file %s\n", argv[i]);
            }

        }


    }else if (strncmp(flag, "find", 4) == 0) {
        if (flag_index + 1 >= argc) {
            fprintf(stderr, "Missing search string\n");
            return ERROR_INVALID_ARGS;
        }

        const char *search_string = argv[flag_index + 1];
        int found_any = 0;

        for (int i = 1; i < file_count + 1; i++) {
            int result = 0;
            StatusCode status = processFind(argv[i], search_string, &result);

            if (status != SUCCESS) {
                fprintf(stderr, "Error searching in file %s\n", argv[i]);
                continue;
            }

            if (result) {
                printf("String found in file: %s\n", argv[i]);
                found_any = 1;
            }
        }

        if (!found_any) {
            printf("The search string was not found in any of the files.\n");
        }
    }

    return SUCCESS;
}
