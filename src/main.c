#include <string.h>
#include <stdio.h>
#include <switch.h>
#include <stdlib.h>
#include <time.h>
#include "main.h"

char userNames[256][100];
AccountUid userAccounts[10];
int selectedUser = 0;
s32 total_users = 0;

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
static int checkConfig(){
    FILE *file = fopen("sdmc:/switch/NX-Save-Sync/config.json", "r");
    if (!file) {
        printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Config file doesnt exist!\n");
        printf(CONSOLE_ESC(0m));
        fclose(file);
        return 0;
    }
    if (file) {
        char buffer[512];
        size_t bytes_read = fread(buffer, 1, 512 - 1, file);
        buffer[bytes_read] = '\0';
        fclose(file);
        char host[64];
        if (getValue(buffer, "host", host, sizeof(host))) {
            printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Current PC IP: %s\n", host);
            printf(CONSOLE_ESC(0m));
        }
    }
    fclose(file);
    return 1;
}
static void drawSelected(int selected) {
    printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "                                                                           \n" CONSOLE_ESC(0m));
    if (selected == 1) {
        printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Push current save file from switch to pc\n" CONSOLE_ESC(0m));
        printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Start Server                                                                  \n" CONSOLE_ESC(0m));
    } else if (selected == 2) {
        printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Pull newer save file from pc to switch\n" CONSOLE_ESC(0m));
        printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Connect to PC                                                                 \n" CONSOLE_ESC(0m));
    } else if (selected == 3) {
        if (checkConfig() == 0) {
            printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Set PC IP                                                                  \n" CONSOLE_ESC(0m));
        } else {
            printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Change PC IP                                                                  \n" CONSOLE_ESC(0m));
        }        
    }
}
static void createConfig() {
    SwkbdConfig keyboard;
    char inputText[16] = {0};
    Result rc = 0;
    rc = swkbdCreate(&keyboard, 0);
    if (R_SUCCEEDED(rc)) {
        swkbdConfigMakePresetDefault(&keyboard);
        swkbdConfigSetInitialText(&keyboard, inputText);
        swkbdConfigSetStringLenMax(&keyboard, 15);
        swkbdConfigSetOkButtonText(&keyboard, "Submit");
        swkbdConfigSetSubText(&keyboard, "Please input PC IP");
        swkbdConfigSetGuideText(&keyboard, "192.168.XXX.XXX");
        rc = swkbdShow(&keyboard, inputText, sizeof(inputText));
        swkbdClose(&keyboard); 
        if (R_FAILED(rc) || strcmp(inputText, "") == 0) {
            strcpy(inputText, "192.168.0.0");
        }
    }
    FILE *file = fopen("sdmc:/switch/NX-Save-Sync/config.json", "w");
    if (file) {
        fprintf(file, "{\"host\":\"%s\"}", inputText);
        fclose(file);
    }
}
static void getUsers() {
    Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc)) {
        goto cleanup;
    }
    rc = accountGetUserCount(&total_users);
    if (R_FAILED(rc)) {
        goto cleanup;
    }
    AccountUid *user_ids = (AccountUid *)malloc(sizeof(AccountUid) * total_users);
    s32 actual_users = 0;
    rc = accountListAllUsers(user_ids, total_users, &actual_users);
    if (R_FAILED(rc)) {
        free(user_ids);
        goto cleanup;
    }
    AccountProfile profile;
    AccountProfileBase profile_base;
    for (s32 i = 0; i < actual_users; i++) {
        userAccounts[i] = user_ids[i];
        rc = accountGetProfile(&profile, user_ids[i]);
        if (R_FAILED(rc)) {
            continue;
        }
        rc = accountProfileGet(&profile, NULL, &profile_base);
        if (R_FAILED(rc)) {
            accountProfileClose(&profile);
            continue;
        }
        strcpy(userNames[i], profile_base.nickname);
        accountProfileClose(&profile);
    }
    free(user_ids);
    cleanup:
        accountExit();
}
int main(int argc, char **argv) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    int returnValue = 0;
    int selected = 1;
    getUsers();
    drawBorder();
    drawTabs(selected);
    printf(CONSOLE_ESC(1;2H) CONSOLE_ESC(38;5;255m) "NX-Save-Sync v2.0.0\n\n");
    printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Push current save file from switch to pc\n" CONSOLE_ESC(0m));
    printf(CONSOLE_ESC(9;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Start Server                                                                  \n" CONSOLE_ESC(0m));
    printf(CONSOLE_ESC(45;2H) CONSOLE_ESC(38;5;255m));
    printf("Selected user: %s", userNames[selectedUser]);
    printf(CONSOLE_ESC(45;67H) CONSOLE_ESC(38;5;255m));
    AppletType appletType = appletGetAppletType();
    switch (appletType) {
        case AppletType_Application:
            printf(CONSOLE_ESC(38;5;28m) "Mode:Takeover");
            break;
        case AppletType_LibraryApplet:
        case AppletType_OverlayApplet:
        case AppletType_SystemApplet:
            printf(CONSOLE_ESC(38;5;196m) "Mode: Applet ");
            break;
        default:
            printf(CONSOLE_ESC(38;5;196m) "Mode: Unkown");
            break;
    }
    printf(CONSOLE_ESC(0m));
    while(appletMainLoop()){
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            returnValue = 3;
            break;
        }
        if (kDown & HidNpadButton_L) {
            if (selected != 1) {
                selected -= 1;
                drawTabs(selected);
                drawSelected(selected);
            }
        }
        if (kDown & HidNpadButton_R) {
            if (selected != 3) {
                selected += 1;
                drawTabs(selected);
                drawSelected(selected);
            }
        }
        if (kDown & HidNpadButton_ZL) {
            if (selectedUser != 0) {
                selectedUser -= 1;
                clearSelectedUser();
                printf(CONSOLE_ESC(45;2H) CONSOLE_ESC(38;5;255m));
                printf("Selected user: %s", userNames[selectedUser]);
            }
        }
        if (kDown & HidNpadButton_ZR) {
            if (selectedUser != total_users - 1) {
                selectedUser += 1;
                clearSelectedUser();
                printf(CONSOLE_ESC(45;2H) CONSOLE_ESC(38;5;255m));
                printf("Selected user: %s", userNames[selectedUser]);
            }
        }
        if (kDown & HidNpadButton_A) {
            if (selected == 1) {
                returnValue = push();
                break;
            } else if (selected == 2) {
                returnValue = pull();
                break;
            } else if (selected == 3) {
                createConfig();
                drawSelected(selected);
            }
        }
        consoleUpdate(NULL);
    }
    printf("\n");
    if (returnValue == 0) {
        printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "Process ended with an error!\n" CONSOLE_ESC(0m));
    } else if (returnValue == 1) {
        printf(CONSOLE_ESC(38;5;46m) CONSOLE_ESC(1C) "Process ended successfully!\n" CONSOLE_ESC(0m));
    } else if (returnValue == 2) {
        printf(CONSOLE_ESC(38;5;226m) CONSOLE_ESC(1C) "Process was aborted.\n" CONSOLE_ESC(0m));
        svcSleepThread(500000000);
    }
    if (returnValue == 0 || returnValue == 1 || returnValue == 2) {
        printf(CONSOLE_ESC(1C) CONSOLE_ESC(38;5;255m) "Press PLUS key to exit.\n" );
        while(appletMainLoop()){
            padUpdate(&pad);
            u64 kDown = padGetButtonsDown(&pad);
            if (kDown & HidNpadButton_Plus) {
                break;
            }
            consoleUpdate(NULL);
        }
    }
    consoleExit(NULL);
    return 0;
}
