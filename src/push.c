#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include "main.h"
#include "ui.h"
#include "miniz.h"

char titleNames[256][100];
char titleIDS[256][17];
int arrayNum = 0;
int currentPage = 1;
int maxPages = 1;
int selected = 1;
int selectedInPage = 1;
char *pushingTitle = 0;
char *pushingTID = 0;


uint64_t hex_string_to_u64(const char *str) {
    uint64_t result = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        char c = tolower(str[i]);
        if (c >= '0' && c <= '9') {
            result = (result << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            result = (result << 4) | (c - 'a' + 10);
        } else {
            printf("Invalid character in hex string: %c\n", c);
            return 0;
        }
    }
    return result;
}

void drawTitles() {
    printf(CONSOLE_ESC(8;6H));
    for (int i = ((currentPage-1) * 33); i < ((currentPage) * 33); i++) {
        printf("%-70s\n", titleNames[i]);
        printf(CONSOLE_ESC(5C));
    }
}
void clearTitles() {
    printf(CONSOLE_ESC(5;4H));
    for (int i = 0; i < 37; i++) {
        printf("%-73s\n", "");
        printf(CONSOLE_ESC(4C));
    }
}
void drawSelected() {
    printf(CONSOLE_ESC(7;6H));
    for (int i = 0; i < selectedInPage; i++) {
        printf(CONSOLE_ESC(1B));
    }
    printf(CONSOLE_ESC(48;5;20m));
    printf("%-70s\n", titleNames[selected-1]);
    printf(CONSOLE_ESC(48;5;237m));
}
void clearSelected() {
    printf(CONSOLE_ESC(7;6H));
    for (int i = 0; i < selectedInPage; i++) {
        printf(CONSOLE_ESC(1B));
    }
    printf("%-70s\n", titleNames[selected-1]);
}
void getTitleName(u64 titleId, u32 recordCount) {
    NsApplicationControlData *buf = NULL;
    u64 outsize = 0;
    NacpLanguageEntry *langentry = NULL;
    char name[0x201];
    Result rc = 0;
    buf = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
    if (buf == NULL) {
        printf("Failed to allocate memory for NsApplicationControlData.\n");
        return;
    }
    memset(buf, 0, sizeof(NsApplicationControlData));
    rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId, buf, sizeof(NsApplicationControlData), &outsize);
    if (R_FAILED(rc)) {
        printf("Failed to get application control data for Title ID: 0x%lx (Error: 0x%x)\n", titleId, rc);
        free(buf);
        return;
    }
    if (outsize < sizeof(buf->nacp)) {
        printf("Outsize is too small for Title ID: 0x%lx.\n", titleId);
        free(buf);
        return;
    }
    rc = nacpGetLanguageEntry(&buf->nacp, &langentry);
    if (R_FAILED(rc) || langentry == NULL) {
        printf("Failed to load LanguageEntry for Title ID: 0x%lx.\n", titleId);
        free(buf);
        return;
    }
    memset(name, 0, sizeof(name));
    strncpy(name, langentry->name, sizeof(name) - 1);
    strcpy(titleNames[arrayNum],name);
    char titleIdStr[17];
    snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", titleId); 
    strcpy(titleIDS[arrayNum], titleIdStr);
    arrayNum += 1;
    printf(CONSOLE_ESC(6;6H));
    printf("Scanning installed titles, %d of %d",arrayNum, recordCount);
    consoleUpdate(NULL);
    free(buf);
}

void listTitles() {
    NsApplicationRecord *records = malloc(sizeof(NsApplicationRecord) * 256);
    int32_t recordCount = 0;
    Result rc = nsListApplicationRecord(records, 256, 0, &recordCount);
    printf(CONSOLE_ESC(6;6H));
    printf("Scanning installed titles, %d of %d",arrayNum, recordCount);
    consoleUpdate(NULL);
    if (R_SUCCEEDED(rc)) {
        for (int i = 0; i < recordCount; i++) {
            u64 titleId = records[i].application_id;
            getTitleName(titleId, recordCount);
        }
    } else {
        printf("Failed to list application records: 0x%x\n", rc);
    }
    free(records);
}
void createTempFoler(const char *path) {
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

void copySave(const char *src, const char *dest) {
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
                copySave(src_path, dest_path);
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
void zip_directory_recursive(mz_zip_archive *zip_archive, const char *dir_path, const char *base_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        printf("Failed to open directory: %s\n", dir_path);
        return;
    }

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
            zip_directory_recursive(zip_archive, file_path, zip_path);
        } else if (entry->d_type == DT_REG) {
            FILE *file = fopen(file_path, "rb");
            if (!file) {
                printf("Failed to open file: %s\n", file_path);
                continue;
            }
            
            fseek(file, 0, SEEK_END);
            size_t file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            void *file_data = malloc(file_size);
            fread(file_data, 1, file_size, file);
            fclose(file);
            
            if (!mz_zip_writer_add_mem(zip_archive, zip_path, file_data, file_size, MZ_BEST_COMPRESSION)) {
                printf("Failed to add file to zip: %s\n", zip_path);
            }
            
            free(file_data);
        }
    }
    closedir(dir);
}

void zip_directory(const char *dir_path, const char *zip_path) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_writer_init_file(&zip_archive, zip_path, 0)) {
        printf("Failed to initialize zip archive!\n");
        return;
    }

    zip_directory_recursive(&zip_archive, dir_path, "temp");
    
    mz_zip_writer_finalize_archive(&zip_archive);
    mz_zip_writer_end(&zip_archive);
}
int push() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    drawAppMenu();
    consoleUpdate(NULL);
    nsInitialize();
    printf(CONSOLE_ESC(48;5;237m) CONSOLE_ESC(38;5;255m));
    listTitles();
    maxPages += arrayNum / 33;
    printf(CONSOLE_ESC(6;6H));
    printf("                                                                      ");
    printf(CONSOLE_ESC(6;18H)"%s%d%s", "Select a title. Page 1 / ", maxPages, " (L / R - Move pages)");
    drawTitles();
    drawSelected();
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            break;
        }
        if (padGetButtonsDown(&pad) & HidNpadButton_AnyUp) {
            u64 startTime = armGetSystemTick();
            bool held = false;
            while (padGetButtons(&pad) & HidNpadButton_AnyUp) {
                if (armGetSystemTick() - startTime > armGetSystemTickFreq() / 3) {
                    held = true;
                    break;
                }
                padUpdate(&pad);
            }
            if (held) {
                while (padGetButtons(&pad) & HidNpadButton_AnyUp) {
                    if (selectedInPage != 1) {
                        clearSelected();
                        selectedInPage -= 1;
                        selected -= 1;
                        drawSelected();
                    }
                    svcSleepThread(30000000);
                    consoleUpdate(NULL);
                    padUpdate(&pad);
                }
            } else {
                if (selectedInPage != 1) {
                    clearSelected();
                    selectedInPage -= 1;
                    selected -= 1;
                    drawSelected();
                }
            }
        }
        if (padGetButtonsDown(&pad) & HidNpadButton_AnyDown) {
            u64 startTime = armGetSystemTick();
            bool held = false;
            while (padGetButtons(&pad) & HidNpadButton_AnyDown) {
                if (armGetSystemTick() - startTime > armGetSystemTickFreq() / 3) {
                    held = true;
                    break;
                }
                padUpdate(&pad);
            }
            if (held) {
                while (padGetButtons(&pad) & HidNpadButton_AnyDown) {
                    if (selectedInPage != 33) {
                        clearSelected();
                        selectedInPage += 1;
                        selected += 1;
                        drawSelected();
                    }
                    svcSleepThread(30000000);
                    consoleUpdate(NULL);
                    padUpdate(&pad);
                }
            } else {
                if (selectedInPage != 33) {
                    clearSelected();
                    selectedInPage += 1;
                    selected += 1;
                    drawSelected();
                }
            }
        }
        if (kDown & HidNpadButton_L) {
            if (currentPage != 1) {
                clearSelected();
                currentPage -= 1;
                selected = (33 * (currentPage-1) + 1);
                selectedInPage = 1;
                drawTitles();
                drawSelected();
            }
        }
        if (kDown & HidNpadButton_R) {
            if (currentPage != maxPages) {
                clearSelected();
                currentPage += 1;
                selected = (33 * (currentPage-1) + 1);
                selectedInPage = 1;
                drawTitles();
                drawSelected();
            }
        }
        if (kDown & HidNpadButton_A) {
            pushingTitle = titleNames[selected-1];
            pushingTID = titleIDS[selected-1];
            printf(CONSOLE_ESC(0m));
            clearTitles();
            break;
        }
        consoleUpdate(NULL);
    }
    nsExit();
    printf(CONSOLE_ESC(6;1H) CONSOLE_ESC(38;5;255m));
    printf(CONSOLE_ESC(1C) "Selected title: %s with TID %s\n", pushingTitle, pushingTID);

    Result rc=0;
    AccountUid userID={0};
    AccountProfile profile;
    AccountUserData userdata;
    AccountProfileBase profilebase;
    char nickname[0x21];
    memset(&userdata, 0, sizeof(userdata));
    memset(&profilebase, 0, sizeof(profilebase));
    rc = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(rc)) {
        printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "accountInitialize() failed!\n" CONSOLE_ESC(0m));
        return 0;
    }
    if (R_SUCCEEDED(rc)) {
        rc = accountGetPreselectedUser(&userID);
        if (R_FAILED(rc)) {
            PselUserSelectionSettings settings;
            memset(&settings, 0, sizeof(settings));
            rc = pselShowUserSelector(&userID, &settings);
        }
        if (R_FAILED(rc)) {
            printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "pselShowUserSelector() failed!\n" CONSOLE_ESC(0m));
            return 0;
        }
        if (R_SUCCEEDED(rc)) {
            rc = accountGetProfile(&profile, userID);
            if (R_FAILED(rc)) {
                printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "accountGetProfile() failed!\n" CONSOLE_ESC(0m));
                return 0;
            }
        }
        if (R_SUCCEEDED(rc)) {
            rc = accountProfileGet(&profile, &userdata, &profilebase);
            if (R_FAILED(rc)) {
                printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "accountProfileGet() failed!\n" CONSOLE_ESC(0m));
                return 0;
            }
            if (R_SUCCEEDED(rc)) {
                memset(nickname,  0, sizeof(nickname));
                strncpy(nickname, profilebase.nickname, sizeof(nickname)-1);
                printf(CONSOLE_ESC(1C) "Selected user: %s\n", nickname);
                consoleUpdate(NULL);
            }
            accountProfileClose(&profile);
        }
        accountExit();
    }

    uint64_t application_id = hex_string_to_u64(pushingTID);
    if (R_SUCCEEDED(rc)) {  
        rc = fsdevMountSaveData("save", application_id, userID);
        if (R_FAILED(rc)) {
            printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "fsdevMountSaveData() failed!\n" CONSOLE_ESC(0m));
            return 0;
        }
        printf(CONSOLE_ESC(1C) "Mounting save:/\n");
        consoleUpdate(NULL);
    }
    if (R_SUCCEEDED(rc)) {
        char title_id_folder[64];
        snprintf(title_id_folder, sizeof(title_id_folder), "sdmc:/temp/%s/", pushingTID);
        printf(CONSOLE_ESC(1C) "Exporting save data from save:/ to %s\n", title_id_folder);
        consoleUpdate(NULL);
        copySave("save:/", title_id_folder);
        fsdevUnmountDevice("save");
    }
    printf(CONSOLE_ESC(1C) "Zipping sdmc:/temp/ folder\n");
    consoleUpdate(NULL);
    zip_directory("sdmc:/temp/", "sdmc:/temp.zip");
    
    return 1;
}