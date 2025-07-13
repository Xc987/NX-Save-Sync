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
#include <errno.h>
#include "main.h"
#include "miniz.h"
#include "util.h"

static NsApplicationControlData* titleDataBuffer = NULL;
static volatile bool shutdownRequested = false;
static char titleNames[256][100];
static char titleIDS[256][17];
static char uninstalledTitleIDS[256][17];
static float titleSaveSize[256];
static int totalApps = 0;
static int arrayNum = 0;
static int currentPage = 1;
static int maxPages = 1;
static int selected = 1;
static int uninstalledTitles = 0;
static int selectedTitles[256];
static int selectedInPage = 1;
static char *pushingTitle = 0;
static char *pushingTID = 0;

static u64 calculateFolderSize(const char *path) {
    u64 totalSize = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    if ((dir = opendir(path)) == NULL) {
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
        if (stat(fullPath, &statbuf) == -1) {
            continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
            totalSize += calculateFolderSize(fullPath);
        } else {
            totalSize += statbuf.st_size;
        }
    }
    closedir(dir);
    return totalSize;
}
static char* wide_to_utf8(const wchar_t* wide_str) {
    if (!wide_str) return NULL;
    size_t size = wcstombs(NULL, wide_str, 0);
    if (size == (size_t)-1) return NULL;
    char* utf8_str = malloc(size + 1);
    if (!utf8_str) return NULL;
    size_t converted = wcstombs(utf8_str, wide_str, size + 1);
    if (converted == (size_t)-1) {
        free(utf8_str);
        return NULL;
    }
    utf8_str[converted] = '\0';
    return utf8_str;
}
static int sendAll(int socket, const void *buffer, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        int result = send(socket, (char*)buffer + sent, length - sent, 0);
        if (result <= 0) return result;
        sent += result;
    }
    return sent;
}
static void handleHttp(int client_socket) {
    int buf_size = 512 * 1024;
    setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
    struct timeval timeout = {30, 0};
    setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';
    if (strstr(buffer, "SHUTDOWN") != NULL) {
        shutdownRequested = true;
        const char *response = "HTTP/1.1 200 OK\r\n\r\nServer is shutting down.";
        sendAll(client_socket, response, strlen(response));
        close(client_socket);
        return;
    }
    FILE *file = fopen("/temp.zip", "rb");
    if (!file) {
        const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
        sendAll(client_socket, response, strlen(response));
        close(client_socket);
        return;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    const char *file_name = strrchr("/temp.zip", '/');
    file_name = file_name ? file_name + 1 : "/temp.zip";
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/zip\r\n"
             "Content-Disposition: attachment; filename=\"%s\"\r\n"
             "Content-Length: %ld\r\n"
             "Connection: keep-alive\r\n\r\n",
             file_name, file_size);
    if (sendAll(client_socket, header, strlen(header)) < 0) {
        fclose(file);
        close(client_socket);
        return;
    }
    #define BUF_SIZE (64 * 1024)
    char file_buf[BUF_SIZE];
    size_t total_sent = 0;
    while (total_sent < file_size) {
        size_t to_read = (file_size - total_sent) < BUF_SIZE ? 
                         (file_size - total_sent) : BUF_SIZE;
                         
        size_t bytes_read = fread(file_buf, 1, to_read, file);
        if (bytes_read == 0) break;

        int bytes_sent = sendAll(client_socket, file_buf, bytes_read);
        if (bytes_sent <= 0) {
            break;
        }
        total_sent += bytes_sent;
    }
    fclose(file);
    usleep(100000);
    close(client_socket);
}
static int startSend() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    socketInitializeDefault();
    u32 local_ip = gethostid();
    u32 correct_ip = __builtin_bswap32(local_ip);
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
             (correct_ip >> 24) & 0xFF,
             (correct_ip >> 16) & 0xFF,
             (correct_ip >> 8) & 0xFF,
             correct_ip & 0xFF);
    printf(CONSOLE_ESC(1C) "Switch IP: " CONSOLE_ESC(38;5;226m));
    printf("%s\n", ip_str);
    consoleUpdate(NULL);
    printf(CONSOLE_ESC(38;5;255m));
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf(CONSOLE_ESC(1C) "Failed to create socket.\n");
        return 0;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8080);
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf(CONSOLE_ESC(1C) "Failed to bind socket.\n");
        close(server_socket);
        return 0;
    }
    if (listen(server_socket, 5) < 0) {
        printf(CONSOLE_ESC(1C) "Failed to listen on socket.\n");
        close(server_socket);
        return 0;
    }
    printf(CONSOLE_ESC(1C) "Server is now running\n");
    consoleUpdate(NULL);
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
    bool exitflag = false;
    while (!shutdownRequested) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus || kDown & HidNpadButton_B) {
            shutdownRequested = true;
            exitflag = true;
            break;
        }
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                usleep(100000);
                continue;
            }
            printf(CONSOLE_ESC(1C) "Failed to accept connection.\n");
            continue;
        }
        flags = fcntl(client_socket, F_GETFL, 0);
        fcntl(client_socket, F_SETFL, flags & ~O_NONBLOCK);
        handleHttp(client_socket);
    }
    printf(CONSOLE_ESC(1C) "Shutting down server\n");
    consoleUpdate(NULL);
    close(server_socket);
    socketExit();
    if (exitflag == true) {
        return 2;
    }
    return 1;
}
static void zipDirRec(mz_zip_archive *zip_archive, const char *dir_path, const char *base_path, int *zipped_files, int total_files) {
    DIR* dir = opendir(dir_path);
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
        char zip_path[PATH_MAX];
        snprintf(zip_path, sizeof(zip_path), "%s/%s", base_path, entry->d_name);
        if (entry->d_type == DT_DIR) {
            zipDirRec(zip_archive, file_path, zip_path, zipped_files, total_files);
        } else if (entry->d_type == DT_REG) {
            printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C));
            printf("Zipping sdmc:/temp/ folder - %d / %d (%.2f%%)\n", *zipped_files, total_files, ((float)*zipped_files / total_files) * 100);
            consoleUpdate(NULL);
            FILE *file = fopen(file_path, "rb");
            if (!file) {
                printf(CONSOLE_ESC(1C) "\nFailed to open file: %s\n", entry->d_name);
                continue;
            }
            fseek(file, 0, SEEK_END);
            size_t file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            void *file_data = malloc(file_size);
            fread(file_data, 1, file_size, file);
            fclose(file);
            if (getKeyValue("compression") == true) {
                if (mz_zip_writer_add_mem(zip_archive, zip_path, file_data, file_size, MZ_BEST_SPEED)) {
                    (*zipped_files)++;
                    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C));
                    printf("Zipping sdmc:/temp/ folder - %d / %d (%.2f%%)\n", *zipped_files, total_files, ((float)*zipped_files / total_files) * 100);
                    consoleUpdate(NULL);
                } else {
                    printf(CONSOLE_ESC(1C) "\nFailed to add file to zip: %s\n", entry->d_name);
                }
            } else {
                if (mz_zip_writer_add_mem(zip_archive, zip_path, file_data, file_size, MZ_NO_COMPRESSION)) {
                    (*zipped_files)++;
                    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C));
                    printf("Zipping sdmc:/temp/ folder - %d / %d (%.2f%%)\n", *zipped_files, total_files, ((float)*zipped_files / total_files) * 100);
                    consoleUpdate(NULL);
                } else {
                    printf(CONSOLE_ESC(1C) "\nFailed to add file to zip: %s\n", entry->d_name);
                }
            }
            
            free(file_data);
        }
    }
    closedir(dir);
}
static void zipDir(const char *dir_path, const char *zip_path) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    int zipped_files = 0;
    int total_files = countFilesRec(dir_path);
    if (!mz_zip_writer_init_file(&zip_archive, zip_path, 0)) {
        printf(CONSOLE_ESC(1C) "Failed to initialize zip archive!\n");
        return;
    }
    zipDirRec(&zip_archive, dir_path, "temp", &zipped_files, total_files);
    mz_zip_writer_finalize_archive(&zip_archive);
    mz_zip_writer_end(&zip_archive);
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "                                                                      \n");
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Zipping sdmc:/temp/ folder\n");
    consoleUpdate(NULL);
}
static void drawTitles() {
    printf(CONSOLE_ESC(9;6H));
    bool uninstalled = false;
    for (int i = ((currentPage-1) * 29); i < ((currentPage) * 29); i++) {
        for (int n = 0; n < uninstalledTitles; n++){
            if (strcmp(titleIDS[i], uninstalledTitleIDS[n]) == 0) {
                uninstalled = true;
            }
        }
        if (!uninstalled) {
            printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        } else {
            printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;196m));
        }
        printf("%-70s\n", titleNames[i]);
        if (totalApps > i) {
            printf(CONSOLE_ESC(63C)CONSOLE_ESC(1A));
            if (titleSaveSize[i] > 0.1f) {
                printf("%*.2f MB\n", 9, titleSaveSize[i]);
            } else if (titleSaveSize[i] * (1024.0f * 1024.0f) > 0.1f) {
                printf("%*.2f KB\n", 9, titleSaveSize[i] * 1024.0f);
            } else {
                printf(CONSOLE_ESC(2C));
                printf("%*s\n", 9, "empty save");
            }
        }
        printf(CONSOLE_ESC(5C));
        uninstalled = false;
    }
    printf(CONSOLE_ESC(0m));
}
static void clearTitles() {
    printf(CONSOLE_ESC(6;4H));
    for (int i = 0; i < 35; i++) {
        printf("%-73s\n", "");
        printf(CONSOLE_ESC(4C));
    }
    printf(CONSOLE_ESC(0m));
}
static void drawMultipleSelected() {
    int savedSelected = selected;
    for (int line = 1; line < 30; line++) {
        printf(CONSOLE_ESC(8;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        for (int i = 0; i < line; i++) {
            printf(CONSOLE_ESC(1B));
        }
        bool found = false;
        bool uninstalled = false;
        for (int i = 0; i < arrayNum + 1; i++) {
            if (selectedTitles[i] == savedSelected) {
                for (int n = 0; n < uninstalledTitles; n++){
                    if (strcmp(titleIDS[savedSelected-1], uninstalledTitleIDS[n]) == 0) {
                        uninstalled = true;
                    }
                }
                if (!uninstalled) {
                    printf(CONSOLE_ESC(48;5;32m) CONSOLE_ESC(38;5;255m));
                } else {
                    printf(CONSOLE_ESC(48;5;32m) CONSOLE_ESC(38;5;196m));
                }
                printf("%-70s\n", titleNames[savedSelected-1]);
                printf(CONSOLE_ESC(63C)CONSOLE_ESC(1A));
                if (titleSaveSize[savedSelected-1] > 0.1f) {
                    printf("%*.2f MB\n", 9, titleSaveSize[savedSelected-1]);
                } else if (titleSaveSize[savedSelected-1] * (1024.0f * 1024.0f) > 0.1f) {
                    printf("%*.2f KB\n", 9, titleSaveSize[savedSelected-1] * 1024.0f);
                } else {
                    printf(CONSOLE_ESC(2C));
                    printf("%*s\n", 9, "empty save");
                }
                found = true;
                break;
            }
        }
        uninstalled = false;
        if (found == false) {
            for (int n = 0; n < uninstalledTitles; n++){
                if (strcmp(titleIDS[savedSelected-1], uninstalledTitleIDS[n]) == 0) {
                    uninstalled = true;
                }
            }
            if (!uninstalled) {
                printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
            } else {
                printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;196m));
            }
            printf("%-70s\n", titleNames[savedSelected-1]);
            if (totalApps >= savedSelected){
                printf(CONSOLE_ESC(63C)CONSOLE_ESC(1A));
                if (titleSaveSize[savedSelected-1] > 0.1f) {
                    printf("%*.2f MB\n", 9, titleSaveSize[savedSelected-1]);
                } else if (titleSaveSize[savedSelected-1] * (1024.0f * 1024.0f) > 0.1f) {
                    printf("%*.2f KB\n", 9, titleSaveSize[savedSelected-1] * 1024.0f);
                } else {
                    printf(CONSOLE_ESC(2C));
                    printf("%*s\n", 9, "empty save");
                }
            }
        }
        savedSelected += 1;
        printf(CONSOLE_ESC(0m));
    }
}
static void drawSelected() {
    printf(CONSOLE_ESC(8;6H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m));
    for (int i = 0; i < selectedInPage; i++) {
        printf(CONSOLE_ESC(1B));
    }
    bool found = false;
    bool uninstalled = false;
    for (int i = 0; i < arrayNum + 1; i++) {
        if (selectedTitles[i] == selected){
            for (int n = 0; n < uninstalledTitles; n++){
                if (strcmp(titleIDS[selected-1], uninstalledTitleIDS[n]) == 0) {
                        uninstalled = true;
                }
            }
            if (!uninstalled) {
                printf(CONSOLE_ESC(48;5;26m) CONSOLE_ESC(38;5;255m));
            } else {
                printf(CONSOLE_ESC(48;5;26m) CONSOLE_ESC(38;5;196m));
            }
            printf("%-70s\n", titleNames[selected-1]);
            printf(CONSOLE_ESC(63C)CONSOLE_ESC(1A));
            if (titleSaveSize[selected-1] > 0.1f) {
                printf("%*.2f MB\n", 9, titleSaveSize[selected-1]);
            } else if (titleSaveSize[selected-1] * (1024.0f * 1024.0f) > 0.1f) {
                printf("%*.2f KB\n", 9, titleSaveSize[selected-1] * 1024.0f);
            } else {
                printf(CONSOLE_ESC(2C));
                printf("%*s\n", 9, "empty save");
            }
            found = true;
            uninstalled = false;
            break;
        }
    }
    uninstalled = false;
    if (found == false) {
        for (int n = 0; n < uninstalledTitles; n++){
            if (strcmp(titleIDS[selected-1], uninstalledTitleIDS[n]) == 0) {
                uninstalled = true;
            }
        }
        if (!uninstalled) {
            printf(CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m));
        } else {
            printf(CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;196m));
        }
        printf("%-70s\n", titleNames[selected-1]);
        printf(CONSOLE_ESC(63C)CONSOLE_ESC(1A));
        if (titleSaveSize[selected-1] > 0.1f) {
            printf("%*.2f MB\n", 9, titleSaveSize[selected-1]);
        } else if (titleSaveSize[selected-1] * (1024.0f * 1024.0f) > 0.1f) {
            printf("%*.2f KB\n", 9, titleSaveSize[selected-1] * 1024.0f);
        } else {
            printf(CONSOLE_ESC(2C));
            printf("%*s\n", 9, "empty save");
        }
    }
    printf(CONSOLE_ESC(0m));
}
static void clearSelected() {
    printf(CONSOLE_ESC(8;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
    for (int i = 0; i < selectedInPage; i++) {
        printf(CONSOLE_ESC(1B));
    }
    bool found = false;
    bool uninstalled = false;
    for (int i = 0; i < arrayNum + 1; i++) {
        if (selectedTitles[i] == selected) {
            for (int n = 0; n < uninstalledTitles; n++){
                if (strcmp(titleIDS[selected-1], uninstalledTitleIDS[n]) == 0) {
                        uninstalled = true;
                }
            }
            if (!uninstalled) {
                printf(CONSOLE_ESC(48;5;32m) CONSOLE_ESC(38;5;255m));
            } else {
                printf(CONSOLE_ESC(48;5;32m) CONSOLE_ESC(38;5;196m));
            }
            printf("%-70s\n", titleNames[selected-1]);
            printf(CONSOLE_ESC(63C)CONSOLE_ESC(1A));
            if (titleSaveSize[selected-1] > 0.1f) {
                printf("%*.2f MB\n", 9, titleSaveSize[selected-1]);
            } else if (titleSaveSize[selected-1] * (1024.0f * 1024.0f) > 0.1f) {
                printf("%*.2f KB\n", 9, titleSaveSize[selected-1] * 1024.0f);
            } else {
                printf(CONSOLE_ESC(2C));
                printf("%*s\n", 9, "empty save");
            }
            found = true;
            uninstalled = false;
            break;
        }
    }
    if (found == false) {
        for (int n = 0; n < uninstalledTitles; n++){
            if (strcmp(titleIDS[selected-1], uninstalledTitleIDS[n]) == 0) {
                uninstalled = true;
            }
        }
        if (!uninstalled) {
            printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        } else {
            printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;196m));
        }
        printf("%-70s\n", titleNames[selected-1]);
        printf(CONSOLE_ESC(63C)CONSOLE_ESC(1A));
        if (titleSaveSize[selected-1] > 0.1f) {
            printf("%*.2f MB\n", 9, titleSaveSize[selected-1]);
        } else if (titleSaveSize[selected-1] * (1024.0f * 1024.0f) > 0.1f) {
            printf("%*.2f KB\n", 9, titleSaveSize[selected-1] * 1024.0f);
        } else {
            printf(CONSOLE_ESC(2C));
            printf("%*s\n", 9, "empty save");
        }
    }
    printf(CONSOLE_ESC(0m));
}
static void initializeTitleBuffer() {
    if (titleDataBuffer == NULL) {
        titleDataBuffer = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
        if (titleDataBuffer == NULL) {
            printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "Failed to allocate persistent title buffer.\n" CONSOLE_ESC(0m));
        }
    }
}
static void getTitleName(u64 titleId, u32 recordCount) {
    u64 outsize = 0;
    NacpLanguageEntry *langentry = NULL;
    char name[0x201];
    Result rc = 0;
    if (titleDataBuffer == NULL) {
        initializeTitleBuffer();
        if (titleDataBuffer == NULL) return;
    }
    memset(titleDataBuffer, 0, sizeof(NsApplicationControlData));
    rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId, titleDataBuffer, sizeof(NsApplicationControlData), &outsize);
    if (R_FAILED(rc)) {
        printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "Failed to get control data for %016lX.\n" CONSOLE_ESC(0m), titleId);
        return;
    }
    if (outsize < sizeof(titleDataBuffer->nacp)) {
        printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "Outsize small for %016lX.\n" CONSOLE_ESC(0m), titleId);
        return;
    }
    rc = nacpGetLanguageEntry(&titleDataBuffer->nacp, &langentry);
    if (R_FAILED(rc) || langentry == NULL) {
        printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "No lang entry for %016lX.\n" CONSOLE_ESC(0m), titleId);
        return;
    }
    strncpy(name, langentry->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (arrayNum < 255) {
        strncpy(titleNames[arrayNum], name, sizeof(titleNames[0]) - 1);
        titleNames[arrayNum][sizeof(titleNames[0]) - 1] = '\0';
        snprintf(titleIDS[arrayNum], sizeof(titleIDS[0]), "%016lX", titleId);
        arrayNum++;
        printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        printf("Scanning installed titles, %d of %d",arrayNum, recordCount);
        printf(CONSOLE_ESC(0m));
        consoleUpdate(NULL);
    }
}
void cleanupTitleBuffer() {
    if (titleDataBuffer) {
        free(titleDataBuffer);
        titleDataBuffer = NULL;
    }
}
bool uninstalledTitle(u64 titleID, char *name, size_t name_size) {
    NsApplicationControlData *buf = malloc(sizeof(NsApplicationControlData));
    if (!buf) return false;
    memset(buf, 0, sizeof(NsApplicationControlData));

    u64 outsize = 0;
    NacpLanguageEntry *langentry = NULL;
    bool success = false;

    if (R_SUCCEEDED(nsInitialize())) {
        if (R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, titleID, buf, sizeof(NsApplicationControlData), &outsize))) {
            if (outsize >= sizeof(buf->nacp) && R_SUCCEEDED(nacpGetLanguageEntry(&buf->nacp, &langentry)) && langentry) {
                strncpy(name, langentry->name, name_size - 1);
                name[name_size - 1] = '\0';
                success = true;
            }
        }
        nsExit();
    }

    free(buf);
    return success;
}
static void listTitles() {
    NsApplicationRecord *records = malloc(sizeof(NsApplicationRecord) * 256);
    int32_t recordCount = 0;
    Result rc = nsListApplicationRecord(records, 256, 0, &recordCount);
    printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
    printf("Scanning installed titles, %d of %d",arrayNum, recordCount);
    printf(CONSOLE_ESC(0m));
    consoleUpdate(NULL);
    if (R_SUCCEEDED(rc)) {
        for (int i = 0; i < recordCount; i++) {
            u64 titleId = records[i].application_id;
            Result rc=0;
            if (R_SUCCEEDED(rc)) {
                if (total_users - selectedUser == 2) {
                    rc = fsdevMountDeviceSaveData("save", titleId);
                } else if (total_users - selectedUser == 1) {
                    rc = fsdevMountBcatSaveData("save", titleId);
                } else {
                    rc = fsdevMountSaveData("save", titleId, userAccounts[selectedUser]);
                }
                if (R_SUCCEEDED(rc)) {
                    u64 sizeInBytes = calculateFolderSize("save:/");
                    float size = (float)sizeInBytes / (1024.0f * 1024.0f);
                    titleSaveSize[arrayNum] = size;
                    fsdevUnmountDevice("save");
                    totalApps += 1;
                    getTitleName(titleId, recordCount);
                }
            }
        }
    } else {
        printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "Failed to list application records.\n" CONSOLE_ESC(0m));
    }
    free(records);
    if (getKeyValue("scanuninstalled") == true) {
        accountInitialize(AccountServiceType_System);
        fsInitialize();
        FsSaveDataInfoReader reader;
        fsOpenSaveDataInfoReader(&reader, FsSaveDataSpaceId_User);
        FsSaveDataInfo info;
        char name[0x201];
        bool found = false;
        printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        printf("                                                           ");
        printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
        printf(CONSOLE_ESC(0m));
        consoleUpdate(NULL);
        while (true) {
            s64 entries_read = 0;
            Result rc = fsSaveDataInfoReaderRead(&reader, &info, 1, &entries_read);
            if (R_FAILED(rc) || entries_read == 0) break;

            if (info.save_data_type == FsSaveDataType_Account && memcmp(&info.uid, &userAccounts[selectedUser], sizeof(AccountUid)) == 0 ) {
                if (uninstalledTitle(info.application_id, name, sizeof(name))) {
                    for (int i = 0; i < arrayNum; i++) {
                        if (strtoull(titleIDS[i], NULL, 16) == info.application_id) {
                            found = true;
                        }
                    }
                    if (!found) {
                        strncpy(titleNames[arrayNum], name, sizeof(titleNames[0]) - 1);
                        titleNames[arrayNum][sizeof(titleNames[0]) - 1] = '\0';
                        snprintf(titleIDS[arrayNum], sizeof(titleIDS[0]), "%016lX", info.application_id);
                        snprintf(uninstalledTitleIDS[uninstalledTitles], sizeof(uninstalledTitleIDS[0]), "%016lX", info.application_id);
                        if (total_users - selectedUser == 2) {
                            rc = fsdevMountDeviceSaveData("save", info.application_id);
                        } else if (total_users - selectedUser == 1) {
                            rc = fsdevMountBcatSaveData("save", info.application_id);
                        } else {
                            rc = fsdevMountSaveData("save", info.application_id, userAccounts[selectedUser]);
                        }
                        if (R_SUCCEEDED(rc)) {
                            u64 sizeInBytes = calculateFolderSize("save:/");
                            float size = (float)sizeInBytes / (1024.0f * 1024.0f);
                            titleSaveSize[arrayNum] = size;
                            fsdevUnmountDevice("save");
                        }
                        arrayNum++;
                        totalApps += 1;
                        uninstalledTitles += 1;
                        printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
                        printf("Scanning uninstalled titles, found %d", uninstalledTitles);
                        printf(CONSOLE_ESC(0m));
                        consoleUpdate(NULL);
                    }
                    found = false;
                }
            }
        }
    }
}
static void createTempFoler(const char *path) {
    char tmp[512];
    char *p = NULL;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}
static void copyFiles(const char *src, const char *dest, int *current, int total) {
    DIR *dir;
    struct dirent *ent;
    char src_path[512];
    char dest_path[512];
    if ((dir = opendir(src)) != NULL) {
        createTempFoler(dest);
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            snprintf(src_path, sizeof(src_path), "%s/%s", src, ent->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, ent->d_name);
            if (ent->d_type == DT_DIR) {
                copyFiles(src_path, dest_path, current, total);
            } else {
                FILE *src_file = fopen(src_path, "rb");
                FILE *dest_file = fopen(dest_path, "wb");
                if (src_file && dest_file) {
                    char buffer[131072];
                    
                    size_t bytes;
                    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                        fwrite(buffer, 1, bytes, dest_file);
                    }
                    fclose(src_file);
                    fclose(dest_file);
                    (*current)++;
                    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C));
                    printf("Exporting save data - %d / %d (%.2f%%)\n", *current, total, ((float)*current / total) * 100);
                    consoleUpdate(NULL);
                } else {
                    printf("Failed to copy: %s\n", src_path);
                }
            }
        }
        closedir(dir);
    } else {
        printf("Failed to open directory: %s\n", src);
    }
}
static void copySave(const char *src, const char *dest) {
    int totalFiles = countFilesRec(src);
    int current = 0;
    copyFiles(src, dest, &current, totalFiles);
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "                                                                      \n");
    printf(CONSOLE_ESC(1A) CONSOLE_ESC(1C) "Exporting save data\n");
    consoleUpdate(NULL);
}
static bool isButtonHeld(PadState* pad, u64 button) {
    u64 startTime = armGetSystemTick();
    bool held = false;
    while (padGetButtons(pad) & button) {
        if (armGetSystemTick() - startTime > armGetSystemTickFreq() / 3) {
            held = true;
            break;
        }
        padUpdate(pad);
    }
    return held;
}
static void handleMovement(PadState* pad, bool isUp, int* selectedInPage, int* selected, int totalApps) {
    bool held = isButtonHeld(pad, isUp ? HidNpadButton_AnyUp : HidNpadButton_AnyDown);
    int minSel = 1;
    int maxSelPage = 29;
    if (held) {
        while (padGetButtons(pad) & (isUp ? HidNpadButton_AnyUp : HidNpadButton_AnyDown)) {
            if ((isUp && *selectedInPage != minSel) || (!isUp && *selectedInPage != maxSelPage && *selected != totalApps)) {
                clearSelected();
                *selectedInPage += isUp ? -1 : 1;
                *selected += isUp ? -1 : 1;
                drawSelected();
            }
            svcSleepThread(30000000);
            consoleUpdate(NULL);
            padUpdate(pad);
        }
    } else {
        if ((isUp && *selectedInPage != minSel) || (!isUp && *selectedInPage != maxSelPage && *selected != totalApps)) {
            clearSelected();
            *selectedInPage += isUp ? -1 : 1;
            *selected += isUp ? -1 : 1;
            drawSelected();
        }
    }
}
void cleanUpVar() {
    shutdownRequested = false;
    for (int i = 0; i < 256; i++) {
        memset(titleNames[i], 0, 100);
    }
    for (int i = 0; i < 256; i++) {
        memset(titleIDS[i], 0, 100);
    }
    for (int i = 0; i < 256; i++) {
        memset(uninstalledTitleIDS[i], 0, 100);
    }
    for (int i = 0; i < 256; i++) {
        titleSaveSize[i] = 0.0f;
    }
    totalApps = 0;
    arrayNum = 0;
    currentPage = 1;
    maxPages = 1;
    selected = 1;
    for (int i = 0; i < 256; i++) {
        selectedTitles[i] = 0;
    }
    selectedInPage = 1;
    uninstalledTitles = 0;
}
int push() {
    printf(CONSOLE_ESC(10;2H));
    for (int i = 0; i < 20; i++) {
        printf("                                                                  \n");
        printf(CONSOLE_ESC(1C));
    }
    printf(CONSOLE_ESC(11;1H) CONSOLE_ESC(38;5;255m));
    setlocale(LC_ALL, "en_US.UTF-8");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    drawAppMenu();
    consoleUpdate(NULL);
    nsInitialize();
    listTitles();
    cleanupTitleBuffer();
    if (arrayNum == 0){
        printf(CONSOLE_ESC(0m));
        clearTitles();
        printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Send save file to pc or secondary switch\n" CONSOLE_ESC(0m));
        printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Start Server                                                                  \n" CONSOLE_ESC(0m));
        cleanUpVar();
        return 0;
    }
    maxPages += arrayNum / 29;
    if (arrayNum % 29 == 0) {
        maxPages -= 1;
    }
    arrayNum = 0;
    printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
    printf("                                                                      ");
    printf(CONSOLE_ESC(7;28H)"%s%d", "Select a title. Page 1 / ", maxPages);
    printf(CONSOLE_ESC(39;6H) "A - Send title | Y - De/Select title | X - Send all titles");
    printf(CONSOLE_ESC(8;6H));
    for (int i = 0; i < 70; i++) {
        printf("%c",196);
    }
    printf(CONSOLE_ESC(38;6H));
    for (int i = 0; i < 70; i++) {
        printf("%c",196);
    }
    drawTitles();
    drawSelected();
    while (true) {
        padUpdate(&pad);
        if (padGetButtons(&pad) & HidNpadButton_A) {
            svcSleepThread(100000);
        } else {
            break;
        }
    }
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus || kDown & HidNpadButton_B) {
            printf(CONSOLE_ESC(0m));
            clearTitles();
            printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Send save file to pc or secondary switch\n" CONSOLE_ESC(0m));
            printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Start Server                                                                  \n" CONSOLE_ESC(0m));
            cleanUpVar();
            return 2;
        }
        if (padGetButtonsDown(&pad) & HidNpadButton_AnyUp) {
            handleMovement(&pad, true, &selectedInPage, &selected, totalApps);
        }
        if (padGetButtonsDown(&pad) & HidNpadButton_AnyDown) {
            handleMovement(&pad, false, &selectedInPage, &selected, totalApps);
        }
        if (kDown & HidNpadButton_AnyLeft) {
            if (currentPage != 1) {
                clearSelected();
                currentPage -= 1;
                selected = (29 * (currentPage-1) + 1);
                selectedInPage = 1;
                drawTitles();
                drawMultipleSelected();
                drawSelected();
                printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
                printf("                                                                      ");
                printf(CONSOLE_ESC(7;28H)"%s%d%s%d", "Select a title. Page ", currentPage, " / ", maxPages);
                printf(CONSOLE_ESC(0m));
            }
        }
        if (kDown & HidNpadButton_AnyRight) {
            if (currentPage != maxPages) {
                clearSelected();
                currentPage += 1;
                selected = (29 * (currentPage-1) + 1);
                selectedInPage = 1;
                drawTitles();
                drawMultipleSelected();
                drawSelected();
                printf(CONSOLE_ESC(7;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
                printf("                                                                      ");
                printf(CONSOLE_ESC(7;28H)"%s%d%s%d", "Select a title. Page ", currentPage, " / ", maxPages);
                printf(CONSOLE_ESC(0m));
            }
        }
        if (kDown & HidNpadButton_A) {
            if (arrayNum == 0){
                selectedTitles[arrayNum++] = selected;
                printf(CONSOLE_ESC(0m));
                clearTitles();
                break;
            } else {
                printf(CONSOLE_ESC(0m));
                clearTitles();
                break;
            }
        }
        if (kDown & HidNpadButton_Y) {
            if (selectedInPage <= 29) {
                bool found = false;
                if (arrayNum == 0) {
                    selectedTitles[arrayNum++] = selected;
                    drawSelected();
                    found = true;
                    if (selectedInPage != 29 && selected != totalApps) {
                        clearSelected();
                        selectedInPage +=  1;
                        selected += 1;
                        drawSelected();
                    }
                } else {
                    for (int i = 0; i < arrayNum + 1; i++) {
                        if (selectedTitles[i] == selected){
                            selectedTitles[i] = selectedTitles[arrayNum-1];
                            selectedTitles[arrayNum-1] = 0;
                            arrayNum -= 1;
                            found = true;
                            drawSelected();
                            break;
                        }
                    }
                }
                if (found == false) {
                    selectedTitles[arrayNum++] = selected;
                    drawSelected();
                    if (selectedInPage != 29 && selected != totalApps) {
                        clearSelected();
                        selectedInPage +=  1;
                        selected += 1;
                        drawSelected();
                    }
                }
                printf(CONSOLE_ESC(39;6H) CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
                printf("                                                                      ");
                if (arrayNum == 0) {
                    printf(CONSOLE_ESC(39;6H) "A - Send title | Y - De/Select title | X - Send all titles");
                } else {
                    printf(CONSOLE_ESC(39;6H) "A - Send %d titles | Y - De/Select title | X - Send all titles", arrayNum);
                }
                if (arrayNum == 0) {
                    printf(CONSOLE_ESC(38;60H));
                    for (int i = 0; i < 16; i++) {
                        printf("%c",196);
                    }
                } else {
                    double savesizesum = 0;
                    for (int i = 0; i < arrayNum; i++) {
                        savesizesum += titleSaveSize[selectedTitles[i] - 1];
                    }
                    char buffer[64];
                    if (savesizesum > 0.1f) {
                        snprintf(buffer, sizeof(buffer), "%.2f", savesizesum);
                    } else if (savesizesum * (1024.0f * 1024.0f) > 0.1f) {
                        snprintf(buffer, sizeof(buffer), "%.2f", savesizesum * 1024.0f);
                    }
                    int num_length = strlen(buffer);
                    if (num_length < 9) {
                        printf(CONSOLE_ESC(38;64H));
                        int padding = 9 - num_length;
                        for (int i = 0; i < padding; i++) {
                            putchar(196);
                        }
                    }
                    if (savesizesum > 0.1f) {
                        printf("%s MB\n", buffer);
                    } else if (savesizesum * (1024.0f * 1024.0f) > 0.1f) {
                        printf("%s KB\n", buffer);
                    }
                }
                printf(CONSOLE_ESC(0m));
            }
            
        }
        if (kDown & HidNpadButton_X) {
            arrayNum = 0;
            for (int i = 0; i < 255 + 1; i++) {
                selectedTitles[i] = 0;
            }
            for (int i = 1; i < totalApps + 1; i++) {
                selectedTitles[arrayNum++] = i;
            }
            printf(CONSOLE_ESC(0m));
            clearTitles();
            break;
        }
        consoleUpdate(NULL);
    }
    nsExit();
    printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Send save file to pc or secondary switch\n" CONSOLE_ESC(0m));
    printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Start Server                                                                  \n" CONSOLE_ESC(0m));
    printf(CONSOLE_ESC(11;1H) CONSOLE_ESC(38;5;255m));
    consoleUpdate(NULL);
    while (true) {
        padUpdate(&pad);
        if (padGetButtons(&pad) & HidNpadButton_A || padGetButtons(&pad) & HidNpadButton_B) {
            svcSleepThread(100000);
        } else {
            break;
        }
    }
    checkTempZip();
    while (true) {
        padUpdate(&pad);
        if (padGetButtons(&pad) & HidNpadButton_A || padGetButtons(&pad) & HidNpadButton_B) {
            svcSleepThread(100000);
        } else {
            break;
        }
    }
    checkTempFolder();
    int succeededTitles = 0;
    for (int i = 0; i < arrayNum; i++) {
        printf(CONSOLE_ESC(11;1H) CONSOLE_ESC(38;5;255m));
        for (int i = 0; i < 10; i++) {
            printf(CONSOLE_ESC(1C)"                                                                        \n");
        }
        pushingTitle = titleNames[selectedTitles[i]-1];
        pushingTID = titleIDS[selectedTitles[i]-1];
        printf(CONSOLE_ESC(11;2H) CONSOLE_ESC(38;5;255m));
        printf("Exporting save data from titles - %d / %d\n\n", i + 1, arrayNum);
        printf(CONSOLE_ESC(1C) "Selected title: %s\n", pushingTitle);
        printf(CONSOLE_ESC(1C) "Selected TID: %s\n", pushingTID);
        Result rc=0;
        uint64_t application_id = hexToU64(pushingTID);
        if (R_SUCCEEDED(rc)) { 
            printf(CONSOLE_ESC(1C) "Mounting save:/\n");
            if (total_users - selectedUser == 2) {
                rc = fsdevMountDeviceSaveData("save", application_id);
            } else if (total_users - selectedUser == 1) {
                rc = fsdevMountBcatSaveData("save", application_id);
            } else {
                rc = fsdevMountSaveData("save", application_id, userAccounts[selectedUser]);
            }
            if (R_FAILED(rc)) {
                printf(CONSOLE_ESC(1C) "fsdevMountSaveData() failed!\n");
                continue;
            }
        }
        char title_id_folder[64];
        if (R_SUCCEEDED(rc)) {
            snprintf(title_id_folder, sizeof(title_id_folder), "sdmc:/temp/%s/", pushingTID);
            printf(CONSOLE_ESC(1C) "Exporting save data\n");
            consoleUpdate(NULL);
            copySave("save:/", title_id_folder);
            fsdevUnmountDevice("save");
        }
        char title_name_folder[128];
        snprintf(title_name_folder, sizeof(title_name_folder), "%sTitle_Name/", title_id_folder);
        mkdir(title_name_folder, 0777);
        char title_name_file[256];
        snprintf(title_name_file, sizeof(title_name_file), "%stitle.txt", title_name_folder);
        wchar_t title_name_wide[256];
        size_t converted_chars = mbstowcs(title_name_wide, pushingTitle, sizeof(title_name_wide)/sizeof(wchar_t));
        char* utf8_str = wide_to_utf8(title_name_wide);
        if (utf8_str) {
            FILE* file = fopen(title_name_file, "w");
            if (file) {
                fwrite(utf8_str, 1, strlen(utf8_str), file);
                fclose(file);
            } else {
                printf(CONSOLE_ESC(1C) "Failed to create title name file!\n");
                continue;
            }
            free(utf8_str);
        } else {
            printf(CONSOLE_ESC(1C) "Failed to convert string to UTF-8!\n");
            continue;
        }
        succeededTitles += 1;
    }
    printf("\n");
    printf(CONSOLE_ESC(1C) "Total moved save files:  %d / %d\n\n", succeededTitles, arrayNum);
    printf(CONSOLE_ESC(1C)"Zipping sdmc:/temp/ folder\n");
    consoleUpdate(NULL);
    zipDir("sdmc:/temp/", "sdmc:/temp.zip");
    socketInitializeDefault();
    Result rc=0;
    rc = nifmInitialize(NifmServiceType_User);
    if (R_FAILED(rc)) {
        printf(CONSOLE_ESC(1C) "Failed to initialize nifm!\n");
        nifmExit();
        socketExit();
        cleanUp();
        cleanUpVar();
        return 0;
    }
    NifmInternetConnectionStatus status;
    rc = nifmGetInternetConnectionStatus(NULL, NULL, &status);
    if (R_FAILED(rc)) {
        printf(CONSOLE_ESC(1C) "Failed to get connection status!\n");
        nifmExit();
        socketExit();
        cleanUp();
        cleanUpVar();
        return 0;
    }
    if (status == NifmInternetConnectionStatus_Connected) {
        nifmExit();
        socketExit();
        int returnvalue = startSend();
        if (returnvalue == 0) {
            cleanUp();
            cleanUpVar();
            return 0;
        } else if (returnvalue == 2) {
            while(true) {
                padUpdate(&pad);
                if (padGetButtons(&pad) & HidNpadButton_Plus || padGetButtons(&pad) & HidNpadButton_B) {
                    svcSleepThread(100000);
                } else {
                    break;
                }
            }
            cleanUp();
            cleanUpVar();
            return 2;
        }
    } else {
        printf(CONSOLE_ESC(1C) "Console is not connected to the internet!\n");
        socketExit();
        cleanUp();
        cleanUpVar();
        return 0;
    }
    cleanUp();
    cleanUpVar();
    return 1;
}