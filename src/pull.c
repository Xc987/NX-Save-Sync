#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include "main.h"
#include "miniz.h"

static void removeDir(const char *path) {
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
static void cleanDir(const char *path) {
    DIR *dir;
    struct dirent *ent;
    char full_path[512];
    if ((dir = opendir(path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }

            snprintf(full_path, sizeof(full_path), "%s/%s", path, ent->d_name);

            if (ent->d_type == DT_DIR) {
                cleanDir(full_path);
                rmdir(full_path);
            } else {
                remove(full_path);
            }
        }
        closedir(dir);
    }
}
static uint64_t hexToU64(const char *str) {
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
static void hexToUpper(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = toupper(str[i]);
    }
}
static void moveSave(const char *src, const char *dest) {
    DIR *dir;
    struct dirent *ent;
    char src_path[512];
    char dest_path[512];
    mkdir(dest, 0755);
    if ((dir = opendir(src)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            snprintf(src_path, sizeof(src_path), "%s/%s", src, ent->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, ent->d_name);
            if (ent->d_type == DT_DIR) {
                moveSave(src_path, dest_path);
            } else {
                FILE *src_file = fopen(src_path, "rb");
                FILE *dest_file = fopen(dest_path, "wb");
                if (src_file && dest_file) {
                    char buffer[8192];
                    size_t bytes;
                    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                        fwrite(buffer, 1, bytes, dest_file);
                    }
                    fclose(src_file);
                    fclose(dest_file);
                    fsdevCommitDevice("save");
                } else {
                    if (src_file) fclose(src_file);
                    if (dest_file) fclose(dest_file);
                    fsdevCommitDevice("save");
                }
            }
        }
        closedir(dir);
    } else {
        printf("Failed to open directory: %s\n", src);
    }
}
static void makeDir(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}
static bool unzip(const char *zip_path, const char *extract_path) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
        if(!mz_zip_reader_init_file(&zip_archive, zip_path, 0)) {
        return false;
    }
    int file_count = (int)mz_zip_reader_get_num_files(&zip_archive);
    if(file_count == 0) {
        mz_zip_reader_end(&zip_archive);
        return false;
    }
    for(int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if(!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            continue;
        }
        if(mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            continue;
        }
        char out_path[256];
        snprintf(out_path, sizeof(out_path), "%s%s", extract_path, file_stat.m_filename);
        char *last_slash = strrchr(out_path, '/');
        if(last_slash) {
            *last_slash = '\0';
            makeDir(out_path);
            *last_slash = '/';
        }
        if(!mz_zip_reader_extract_to_file(&zip_archive, i, out_path, 0)) {
            continue;
        }
    }
    mz_zip_reader_end(&zip_archive);
    return true;
}
static int getValue(const char *json, const char *key, char *value, size_t value_size) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":\"", key);
    const char *start = strstr(json, search_str);
    if (!start) return 0;
    start += strlen(search_str);
    const char *end = strchr(start, '"');
    if (!end) return 0;
    size_t len = end - start;
    if (len >= value_size) len = value_size - 1;
    strncpy(value, start, len);
    value[len] = '\0';
    return 1;
}
static int downloadZip(char *host) {
    socketInitializeDefault();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf(CONSOLE_ESC(1C) "Socket creation failed\n");
        return 0;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, host, &server_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf(CONSOLE_ESC(1C) "Connection failed\n");
        close(sock);
        socketExit();
        return 0;
    }
    const char *request = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    if (send(sock, request, strlen(request), 0) < 0) {
        printf(CONSOLE_ESC(1C) "Send failed\n");
        close(sock);
        socketExit();
        return 0;
    }
    char buffer[65536];
    FILE *file = fopen("sdmc:/temp.zip", "wb");
    if (!file) {
        printf(CONSOLE_ESC(1C) "Failed to create file\n");
        close(sock);
        socketExit();
        return 0;
    }
    printf(CONSOLE_ESC(1C) "Downloading temp.zip.\n");
    consoleUpdate(NULL);
    int header_ended = 0;
    ssize_t bytes_received;
    size_t total_bytes_received = 0;
    size_t content_length = 0;
    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        if (!header_ended) {
            char *header_end = strstr(buffer, "\r\n\r\n");
            if (header_end) {
                header_ended = 1;
                size_t data_start = header_end - buffer + 4;
                char *content_length_ptr = strstr(buffer, "Content-Length: ");
                if (content_length_ptr) {
                    content_length = strtoul(content_length_ptr + 16, NULL, 10);
                }
                if (bytes_received > data_start) {
                    size_t data_bytes = bytes_received - data_start;
                    fwrite(buffer + data_start, 1, data_bytes, file);
                    total_bytes_received += data_bytes;
                    if (content_length > 0) {
                        double current_mb = (double)total_bytes_received / (1024 * 1024);
                        double total_mb = (double)content_length / (1024 * 1024);
                        printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Downloading temp.zip. %.2f / %.2f MB\n", current_mb, total_mb);
                        consoleUpdate(NULL);
                    }
                }
                continue;
            }
        } else {
            fwrite(buffer, 1, bytes_received, file);
            total_bytes_received += bytes_received;
            
            if (content_length > 0) {
                double current_mb = (double)total_bytes_received / (1024 * 1024);
                double total_mb = (double)content_length / (1024 * 1024);
                printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Downloading temp.zip. %.2f / %.2f MB\n", current_mb, total_mb);
                consoleUpdate(NULL);
            }
        }
    }
    fclose(file);
    close(sock);
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Downloading temp.zip.                                            \n");
    consoleUpdate(NULL);
    int shutdown_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(shutdown_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        send(shutdown_sock, "SHUTDOWN", 8, 0);
        printf(CONSOLE_ESC(1C) "Shutting down server.\n");
        consoleUpdate(NULL);
        char response[128];
        recv(shutdown_sock, response, sizeof(response), 0);
    }
    close(shutdown_sock);
    socketExit();
    return 1;
}
static void cleanUp() {
    printf(CONSOLE_ESC(1C) "Deleting sdmc:/temp.zip file\n");
    consoleUpdate(NULL);
    remove("sdmc:/temp.zip");
    printf(CONSOLE_ESC(1C)"Deleting sdmc:/temp/ folder\n");
    consoleUpdate(NULL);
    removeDir("sdmc:/temp/");
}

int pull() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    printf(CONSOLE_ESC(11;1H) CONSOLE_ESC(38;5;255m));
    FILE *file = fopen("sdmc:/switch/NX-Save-Sync/config.json", "r");
    if (!file) {
        printf(CONSOLE_ESC(1C) "PC IP not set!\n");
        return 0;
    }
    Result rc = 0;
    file = fopen("sdmc:/switch/NX-Save-Sync/config.json", "r");
    if (file) {
        char buffer[512];
        size_t bytes_read = fread(buffer, 1, 512 - 1, file);
        buffer[bytes_read] = '\0';
        fclose(file);
        char host[64];
        if (getValue(buffer, "host", host, sizeof(host))) {
            printf(CONSOLE_ESC(1C) "Connecting to host: %s\n", host);
            consoleUpdate(NULL);
            if (downloadZip(host) == 0) {
                return 0;
            }
        }
    } else {
        fclose(file);
    }
    printf(CONSOLE_ESC(1C) "Unzipping temp.zip\n");
    consoleUpdate(NULL);
    FILE *test = fopen("sdmc:/temp.zip", "rb");
    if(!test) {
        printf(CONSOLE_ESC(1C) "temp.zip not found!\n");
        return 0;
    } else {
        fclose(test);
        unzip("sdmc:/temp.zip", "sdmc:/");
    }
    DIR *dir;
    struct dirent *ent;
    char *folderName = NULL;
    int folderCount = 0;
    dir = opendir("sdmc:/temp/");
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s%s", "sdmc:/temp/", ent->d_name);
        DIR *testDir = opendir(fullPath);
        if (testDir != NULL) {
            closedir(testDir);
            folderCount++;
            folderName = strdup(ent->d_name);
        }
    }
    closedir(dir);
    hexToUpper(folderName);
    uint64_t application_id = hexToU64(folderName);
    if (R_SUCCEEDED(rc)) {
        printf(CONSOLE_ESC(1C) "Mounting save:/\n");
        consoleUpdate(NULL);
        rc = fsdevMountSaveData("save", application_id, userAccounts[selectedUser]);
        if (R_FAILED(rc)) {
            printf(CONSOLE_ESC(1C) "fsdevMountSaveData() failed!\n");
            cleanUp();
            return 0;
        }
    }
    if (R_SUCCEEDED(rc)) {
        printf(CONSOLE_ESC(1C) "Deleting any existing save file in save:/\n");
        consoleUpdate(NULL);
        cleanDir("save:/");
        char backup_path[64];
        snprintf(backup_path, sizeof(backup_path), "sdmc:/temp/%016lx", application_id);
        printf(CONSOLE_ESC(1C) "Moving save file\n");
        consoleUpdate(NULL);
        moveSave(backup_path, "save:/");
        rc = fsdevCommitDevice("save");
        if (R_FAILED(rc)) {
            printf("Failed to commit changes: 0x%x\n", rc);
            cleanUp();
            return 0;
        }
        fsdevUnmountDevice("save");
    }
    cleanUp();
    return 1;
}