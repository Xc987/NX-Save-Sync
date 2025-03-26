#include <string.h>
#include <stdio.h>
#include <switch.h>
#include "main.h"

int main(int argc, char **argv) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    int selected = 1;
    int returnValue = 0;
    drawBorder();
    printf(CONSOLE_ESC(1C) CONSOLE_ESC(38;5;255m) "Welcome to NX save sync. Select an option\n");
    printf(CONSOLE_ESC(1C) CONSOLE_ESC(48;5;20m) "Push current save file from switch to pc                                      \n" CONSOLE_ESC(0m));
    printf(CONSOLE_ESC(1C) CONSOLE_ESC(38;5;240m) "Pull newer save file from pc to switch\n" CONSOLE_ESC(0m));
    while(appletMainLoop()){
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            returnValue = 3;
            break;
        }
        if (kDown & HidNpadButton_AnyUp) {
            if (selected != 1) {
                printf(CONSOLE_ESC(3;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Push current save file from switch to pc                                      \n" CONSOLE_ESC(0m));
                printf(CONSOLE_ESC(4;2H) CONSOLE_ESC(38;5;240m) "Pull newer save file from pc to switch                                        \n" CONSOLE_ESC(0m));
                selected -= 1;
            }
        }
        if (kDown & HidNpadButton_AnyDown) {
            if (selected != 2) {
                printf(CONSOLE_ESC(3;2H) CONSOLE_ESC(38;5;240m) "Push current save file from switch to pc                                      \n" CONSOLE_ESC(0m));
                printf(CONSOLE_ESC(4;2H) CONSOLE_ESC(48;5;20m) CONSOLE_ESC(38;5;255m) "Pull newer save file from pc to switch                                        \n" CONSOLE_ESC(0m));
                selected += 1;
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
