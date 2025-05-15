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
#include <jansson.h>
#include "main.h"
#include "miniz.h"

static bool getKeyValue(char* key) {
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
static int countFilesRec(const char *dir_path) {
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
static void cleanDir(const char *path) {
    int processed_files = 0;
    int total_files = countFilesRec(path);
    DIR *dir;
    struct dirent *ent;
    char full_path[512];
    if ((dir = opendir(path))) {
        while ((ent = readdir(dir))) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            snprintf(full_path, sizeof(full_path), "%s/%s", path, ent->d_name);
            if (ent->d_type == DT_DIR) {
                cleanDir(full_path);
                rmdir(full_path);
            } else {
                remove(full_path);
                processed_files++;
                printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C));
                printf("Deleting any existing save file in save:/ - %d / %d (%.2f%%)\n", processed_files, total_files, ((float)processed_files / total_files) * 100);
                consoleUpdate(NULL);
            }
        }
        closedir(dir);
    }
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Deleting any existing save file in save:/                                  \n");
    consoleUpdate(NULL);
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
    int processed_files = 0;
    int total_files = countFilesRec(src);
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
                    char buffer[262144];
                    size_t bytes;
                    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                        fwrite(buffer, 1, bytes, dest_file);
                    }
                    processed_files++;
                    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C));
                    printf("Moving save file - %d / %d (%.2f%%)\n", processed_files, total_files, ((float)processed_files / total_files) * 100);
                    consoleUpdate(NULL);
                    fclose(src_file);
                    fclose(dest_file);
                    fsdevCommitDevice("save");
                } else {
                    processed_files++;
                    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C));
                    printf("Moving save file - %d / %d (%.2f%%)\n", processed_files, total_files, ((float)processed_files / total_files) * 100);
                    consoleUpdate(NULL);
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
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Moving save file                                             \n");
    consoleUpdate(NULL);
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
    int extracted_count = 0;
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
        extracted_count++;
        printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Unzipping temp.zip - %d / %d (%.2f%%)\n", extracted_count, file_count, ((float)extracted_count / file_count) * 100);
        consoleUpdate(NULL);
    }
    mz_zip_reader_end(&zip_archive);
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Unzipping temp.zip                                             \n");
    consoleUpdate(NULL);
    return true;
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
                        printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Downloading temp.zip - %.2f / %.2f MB (%.2f%%)\n", current_mb, total_mb, ((float)current_mb / total_mb) * 100);
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
                printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Downloading temp.zip - %.2f / %.2f MB\n", current_mb, total_mb);
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
    if (getKeyValue("keep") == false) {
        printf(CONSOLE_ESC(1C) "Deleting sdmc:/temp.zip file\n");
        consoleUpdate(NULL);
        remove("sdmc:/temp.zip");
        printf(CONSOLE_ESC(1C)"Deleting sdmc:/temp/ folder\n");
        consoleUpdate(NULL);
        removeDir("sdmc:/temp/");
    }
}
static void getTitleName(u64 title) {
    Result rc=0;
    u64 application_id=title;
    NsApplicationControlData *buf=NULL;
    u64 outsize=0;
    NacpLanguageEntry *langentry = NULL;
    char name[0x201];
    buf = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
    if (buf==NULL) {
        rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        printf("Failed to alloc mem.\n");
    } else {
        memset(buf, 0, sizeof(NsApplicationControlData));
    }
    if (R_SUCCEEDED(rc)) {
        rc = nsInitialize();
        if (R_FAILED(rc)) {
            printf("nsInitialize() failed: 0x%x\n", rc);
        }
    }
    if (R_SUCCEEDED(rc)) {
        rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, application_id, buf, sizeof(NsApplicationControlData), &outsize);
        if (R_FAILED(rc)) {
            printf("nsGetApplicationControlData() failed: 0x%x\n", rc);
        }
        if (outsize < sizeof(buf->nacp)) {
            rc = -1;
            printf("Outsize is too small: 0x%lx.\n", outsize);
        }
        if (R_SUCCEEDED(rc)) {
            rc = nacpGetLanguageEntry(&buf->nacp, &langentry);
            if (R_FAILED(rc) || langentry==NULL) printf("Failed to load LanguageEntry.\n");
        }
        if (R_SUCCEEDED(rc)) {
            memset(name, 0, sizeof(name));
            strncpy(name, langentry->name, sizeof(name)-1);
            printf(CONSOLE_ESC(1C) "Title: %s\n", name);
        }
        nsExit();
    }
    free(buf);
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
    if (file) {
        json_error_t error;
        json_t *root = json_load_file("sdmc:/switch/NX-Save-Sync/config.json", 0, &error);
        if (!root) {
            return 0;
        }
        json_t *host_val = json_object_get(root, "host");
        if (!json_is_string(host_val)) {
            json_decref(root);
            return 0;
        }
        const char *host_str = json_string_value(host_val);
        char *result = strdup(host_str);
        json_decref(root);
        printf(CONSOLE_ESC(1C) "Connecting to host: %s\n", result);
        consoleUpdate(NULL);
        if (downloadZip(result) == 0) {
            return 0;
        }
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
    DIR *dirw;
    struct dirent *entry;
    int count = 0;
    dirw = opendir("sdmc:/temp");
    while ((entry = readdir(dirw)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (entry->d_type == DT_DIR) {
            count++;
        }
    }
    closedir(dirw);
    char *folderName = NULL;
    int currentSubfolder = 1;
    DIR *dir = opendir("sdmc:/temp");
        if (dir != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;
                if (entry->d_type == DT_DIR) {
                    printf(CONSOLE_ESC(16;1H) CONSOLE_ESC(38;5;255m));
                    for (int i = 0; i < 10; i++) {
                        printf(CONSOLE_ESC(1C)"                                                                        \n");
                    }
                    printf(CONSOLE_ESC(16;2H) CONSOLE_ESC(38;5;255m));
                    printf("Moving title save data - %d / %d\n\n", currentSubfolder, count);
                    char *preHexTID = entry->d_name;
                    folderName = entry->d_name;
                    hexToUpper(folderName);
                    uint64_t application_id = hexToU64(folderName);
                    getTitleName(application_id);
                    printf(CONSOLE_ESC(1C) "TID: %s\n", preHexTID);
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
                    currentSubfolder += 1;
                }
            }
            closedir(dir);
        }
    cleanUp();
    return 1;
}