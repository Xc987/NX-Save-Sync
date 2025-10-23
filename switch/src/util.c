#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <wchar.h>
#include <locale.h>
#include <jansson.h>
#include "main.h"

bool getKeyValue(char* key) {
    json_error_t error;
    json_t *root = json_load_file("sdmc:/switch/NX-Save-Sync/config.json", 0, &error);
    if (!root) {
        printf("Error reading config.json: %s (line %d)\n", error.text, error.line);
        return false;
    }
    json_t *del_val = json_object_get(root, key);
    bool result = json_is_true(del_val);

    json_decref(root);
    return result;
}
uint64_t hexToU64(const char *str) {
    uint64_t result = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        char c = tolower(str[i]);
        if (c >= '0' && c <= '9') {
            result = (result << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            result = (result << 4) | (c - 'a' + 10);
        }
    }
    return result;
}
int countFilesRec(const char *dir_path) {
    DIR* dir = opendir(dir_path);
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
        if (entry->d_type == DT_DIR) {
            count += countFilesRec(file_path);
        } else if (entry->d_type == DT_REG) {
            count++;
        }
    }
    closedir(dir);
    return count;
}
void removeDir(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat statbuf;
        if (lstat(full_path, &statbuf) == -1) {
            perror("Error getting file status");
            continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
            removeDir(full_path);
        } else {
            if (remove(full_path) != 0) {
                perror("Error removing file");
            }
        }
    }
    closedir(dir);
    if (rmdir(path) != 0) {
        perror("Error removing directory");
    }
}
void cleanUp() {
    printf(CONSOLE_ESC(1C) "Deleting sdmc:/temp.zip file\n");
    consoleUpdate(NULL);
    remove("sdmc:/temp.zip");
    printf(CONSOLE_ESC(1C)"Deleting sdmc:/temp/ folder\n");
    consoleUpdate(NULL);
    removeDir("sdmc:/temp/");
}
void checkTempZip() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    if (access("sdmc:/temp.zip", F_OK) == 0) {
        drawTempZipWarning();
        printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        printf(CONSOLE_ESC(21;31H) "   temp.zip found!");
        printf(CONSOLE_ESC(23;31H) "A - Delete");
        printf(CONSOLE_ESC(24;31H) "B - Keep");
        consoleUpdate(NULL);
        while(true) {
            padUpdate(&pad);
            if (padGetButtons(&pad) & HidNpadButton_A) {
                remove("sdmc:/temp.zip");
                break;
            }
            if (padGetButtons(&pad) & HidNpadButton_B) {
                break;
            }
        }
        printf(CONSOLE_ESC(0m) CONSOLE_ESC(21;31H));
        printf(CONSOLE_ESC(20;30H));
            for (int i = 0; i < 7; i++) {
            printf("                      \n");
            printf(CONSOLE_ESC(29C));
        }
        consoleUpdate(NULL);
    }
}
void checkTempFolder() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    DIR* dir = opendir("sdmc:/temp/");
    if (dir) {
        closedir(dir);
        drawTempZipWarning();
        printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        printf(CONSOLE_ESC(21;31H) " temp folder found!");
        printf(CONSOLE_ESC(23;31H) "A - Delete");
        printf(CONSOLE_ESC(24;31H) "B - Keep");
        consoleUpdate(NULL);
        while(true) {
            padUpdate(&pad);
            if (padGetButtons(&pad) & HidNpadButton_A) {
                removeDir("sdmc:/temp/");
                break;
            }
            if (padGetButtons(&pad) & HidNpadButton_B) {
                break;
            }
        }
        printf(CONSOLE_ESC(0m) CONSOLE_ESC(21;31H));
        printf(CONSOLE_ESC(20;30H));
            for (int i = 0; i < 7; i++) {
            printf("                      \n");
            printf(CONSOLE_ESC(29C));
        }
        consoleUpdate(NULL);
    }
}