#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include "main.h"
#include "ui.h"

char titleNames[256][100];
char titleIDS[256][17];
int arrayNum = 0;
int currentPage = 1;
int maxPages = 1;
int selected = 1;
int selectedInPage = 1;
char *pushingTitle = 0;
char *pushingTID = 0;

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
        if (kDown & HidNpadButton_AnyUp) {
            if (selectedInPage != 1) {
                clearSelected();
                selectedInPage -= 1;
                selected -= 1;
                drawSelected();
            }
        }
        if (kDown & HidNpadButton_AnyDown) {
            if (selectedInPage != 33) {
                clearSelected();
                selectedInPage += 1;
                selected += 1;
                drawSelected();
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
    printf(CONSOLE_ESC(6;2H));
    printf("Selected title %s with TID %s\n", pushingTitle, pushingTID);
    while(appletMainLoop()){
        consoleUpdate(NULL);
    }
    return 1;
}