#include <string.h>
#include <stdio.h>
#include <switch.h>
#include <stdlib.h>
#include <jansson.h>
#include "main.h"

char userNames[256][100];
AccountUid userAccounts[12];
int selectedUser = 0;
s32 total_users = 0;

static int checkConfig() {
    FILE *file = fopen("sdmc:/switch/NX-Save-Sync/config.json", "r");
    if (!file) {
        printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Config file doesnt exist!\n");
        printf(CONSOLE_ESC(0m));
        fclose(file);
        return 0;
    }
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
        printf(CONSOLE_ESC(7;2H) CONSOLE_ESC(38;5;255m) "Current PC IP: %s\n", result);
        printf(CONSOLE_ESC(0m));
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
    json_t *root = json_object();
    json_object_set_new(root, "host", json_string(inputText));
    FILE *fp = fopen("sdmc:/switch/NX-Save-Sync/config.json", "w");
    if (fp) {
        json_dumpf(root, fp, JSON_INDENT(4));
        fclose(fp);
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
int main() {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    int returnValue = 0;
    int selected = 1;
    getUsers();
    strcpy(userNames[total_users], "Device");
    strcpy(userNames[total_users + 1], "BCAT");
    total_users += 2;
    drawBorder();
    drawTabs(selected);
    printf(CONSOLE_ESC(1;2H) CONSOLE_ESC(38;5;255m) "NX-Save-Sync v2.1.0\n\n");
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
        if (kDown & HidNpadButton_AnyLeft) {
            if (selected != 1) {
                selected -= 1;
                drawTabs(selected);
                drawSelected(selected);
            }
        }
        if (kDown & HidNpadButton_AnyRight) {
            if (selected != 3) {
                selected += 1;
                drawTabs(selected);
                drawSelected(selected);
            }
        }
        if (kDown & HidNpadButton_ZL || kDown & HidNpadButton_L) {
            if (selectedUser != 0) {
                selectedUser -= 1;
                clearSelectedUser();
                printf(CONSOLE_ESC(45;2H) CONSOLE_ESC(38;5;255m));
                printf("Selected user: %s", userNames[selectedUser]);
            }
        }
        if (kDown & HidNpadButton_ZR || kDown & HidNpadButton_R) {
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
