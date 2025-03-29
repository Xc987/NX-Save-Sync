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

void printTime() {
    printf(CONSOLE_ESC(1;61H));
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    printf(CONSOLE_ESC(38;5;255m));
    printf("%02d.%02d.%04d %02d:%02d:%02d",
               timeinfo->tm_mday,
               timeinfo->tm_mon + 1,
               timeinfo->tm_year + 1900,
               timeinfo->tm_hour,
               timeinfo->tm_min,
               timeinfo->tm_sec);
    printf(CONSOLE_ESC(0m));
}

bool stringToAccountUid(const char* uid_str, AccountUid* out_uid) {
    if (strlen(uid_str) != 32) return false;
    
    char high_str[17] = {0};
    char low_str[17] = {0};
    
    memcpy(high_str, uid_str, 16);
    memcpy(low_str, uid_str + 16, 16);
    
    out_uid->uid[1] = strtoull(high_str, NULL, 16);
    out_uid->uid[0] = strtoull(low_str, NULL, 16);
    
    return true;
}

void getUsers() {
        Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc)) {
        printf("Failed to initialize account service: 0x%x\n", rc);
        consoleUpdate(NULL);
        goto cleanup;
    }
    
    rc = accountGetUserCount(&total_users);
    if (R_FAILED(rc)) {
        printf("Failed to get user count: 0x%x\n", rc);
        consoleUpdate(NULL);
        goto cleanup;
    }
    AccountUid *user_ids = (AccountUid *)malloc(sizeof(AccountUid) * total_users);
    s32 actual_users = 0;
    rc = accountListAllUsers(user_ids, total_users, &actual_users);
    if (R_FAILED(rc)) {
        printf("Failed to list users: 0x%x\n", rc);
        consoleUpdate(NULL);
        free(user_ids);
        goto cleanup;
    }
    AccountProfile profile;
    AccountProfileBase profile_base;
    for (s32 i = 0; i < actual_users; i++) {
        userAccounts[i] = user_ids[i];
        rc = accountGetProfile(&profile, user_ids[i]);
        if (R_FAILED(rc)) {
            printf("User %d: Failed to get profile (0x%x)\n", i+1, rc);
            continue;
        }
        rc = accountProfileGet(&profile, NULL, &profile_base);
        if (R_FAILED(rc)) {
            printf("User %d: Failed to get profile base (0x%x)\n", i+1, rc);
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
    int selected = 1;
    int returnValue = 0;
    getUsers();
    drawBorder();
    printf(CONSOLE_ESC(1;2H) CONSOLE_ESC(38;5;255m) "NX-Save-Sync v1.1.0\n\n");
    printf(CONSOLE_ESC(1C) CONSOLE_ESC(38;5;255m) "Select an option.\n");
    printf(CONSOLE_ESC(1C) CONSOLE_ESC(48;5;20m) "Push current save file from switch to pc                                      \n" CONSOLE_ESC(0m));
    printf(CONSOLE_ESC(1C) CONSOLE_ESC(38;5;240m) "Pull newer save file from pc to switch\n" CONSOLE_ESC(0m));
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
    printTime();
    while(appletMainLoop()){
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            returnValue = 3;
            break;
        }
        if (kDown & HidNpadButton_AnyUp) {
            if (selected != 1) {
                printf(CONSOLE_ESC(4;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Push current save file from switch to pc                                      \n" CONSOLE_ESC(0m));
                printf(CONSOLE_ESC(5;2H) CONSOLE_ESC(38;5;240m) "Pull newer save file from pc to switch                                        \n" CONSOLE_ESC(0m));
                selected -= 1;
            }
        }
        if (kDown & HidNpadButton_AnyDown) {
            if (selected != 2) {
                printf(CONSOLE_ESC(4;2H) CONSOLE_ESC(38;5;240m) "Push current save file from switch to pc                                      \n" CONSOLE_ESC(0m));
                printf(CONSOLE_ESC(5;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Pull newer save file from pc to switch                                        \n" CONSOLE_ESC(0m));
                selected += 1;
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
            } else if (selected == 2) {
                returnValue = pull();
            }
            break;
        }
        printTime();
        consoleUpdate(NULL);
    }
    printf("\n");
    if (returnValue == 0) {
        printf(CONSOLE_ESC(38;5;196m) CONSOLE_ESC(1C) "Process ended with an error!\n" CONSOLE_ESC(0m));
    } else if (returnValue == 1) {
        printf(CONSOLE_ESC(38;5;46m) CONSOLE_ESC(1C) "Process ended successfully!\n" CONSOLE_ESC(0m));
    }
    if (returnValue == 0 || returnValue == 1) {
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
