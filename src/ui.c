#include <stdio.h>
#include <switch.h>
#include "main.h"

void drawBorder() {
    printf("%c",218);
    for (int i = 0; i < 78; i++) {
        printf("%c",196);
    }
    printf("%c",191);
    for (int i = 0; i < 43; i++) {
        printf("%c\n",179);
    }
    printf("%c",192);
    for (int i = 0; i < 78; i++) {
        printf("%c",196);
    }
    printf("%c",217);
    printf(CONSOLE_ESC(2;1H));
    for (int i = 0; i < 42; i++) {
        printf(CONSOLE_ESC(80C));
        printf("%c\n",179);
        printf(CONSOLE_ESC(1A));
    }
    printf(CONSOLE_ESC(44;80H));
    printf("%c",179);
    printf(CONSOLE_ESC(2;1H));
}
void drawAppMenu() {
    printf(CONSOLE_ESC(48;5;237m));
    printf(CONSOLE_ESC(5;5H));
    printf("%c",201);
    for (int i = 0; i < 70; i++) {
        printf("%c",205);
    }
    printf("%c",187);
    printf(CONSOLE_ESC(6;5H));
    for (int i = 0; i < 35; i++) {
        printf("%c\n",186);
        printf(CONSOLE_ESC(4C));
    }
    printf("%c",200);
    for (int i = 0; i < 70; i++) {
        printf("%c",205);
    }
    printf("%c",188);
    printf(CONSOLE_ESC(6;1H));
    for (int i = 0; i < 35; i++) {
        printf(CONSOLE_ESC(75C));
        printf("%c\n",186);
    }
    printf(CONSOLE_ESC(6;6H));
    for (int i = 0; i < 35; i++) {
        printf("                                                                      \n");
        printf(CONSOLE_ESC(5C));
    }
    printf(CONSOLE_ESC(48;5;0m));
}
void clearSelectedUser() {
    printf(CONSOLE_ESC(45;2H));
    for (int i = 0; i < 25; i++) {
        printf("%c",196);
    }
}