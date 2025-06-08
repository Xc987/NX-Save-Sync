#include <stdio.h>
#include <switch.h>
#include "main.h"

void drawBorder() {
    printf(CONSOLE_ESC(1;1H) CONSOLE_ESC(38;5;255m));
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
void drawTabs(int selected) {
    printf(CONSOLE_ESC(3;1H) CONSOLE_ESC(38;5;255m));
    printf(CONSOLE_ESC(1C) "%s","                                                                              \n");
    printf(CONSOLE_ESC(1C) "%s","                                                                              \n");
    printf(CONSOLE_ESC(1C) "%s","                                                                              \n");
    if (selected == 1) {
        printf(CONSOLE_ESC(3;4H));
        printf("%c",218);
        for (int i = 0; i < 22; i++) {
            printf("%c",196);
        }
        printf("%c",191);
        printf(CONSOLE_ESC(4;4H));
        printf("%c",179);
        printf(CONSOLE_ESC(4;27H));
        printf("%c",179);
        printf(CONSOLE_ESC(5;2H));
        printf("%c",196);
        printf("%c",196);
        printf("%c",217);
        printf(CONSOLE_ESC(5;27H));
        printf("%c",192);
        for (int i = 0; i < 52; i++) {
            printf("%c",196);
        }
    } else if (selected == 2) {
        printf(CONSOLE_ESC(3;29H));
        printf("%c",218);
        for (int i = 0; i < 22; i++) {
            printf("%c",196);
        }
        printf("%c",191);
        printf(CONSOLE_ESC(4;29H));
        printf("%c",179);
        printf(CONSOLE_ESC(4;52H));
        printf("%c",179);
        printf(CONSOLE_ESC(5;2H));
        for (int i = 0; i < 27; i++) {
            printf("%c",196);
        }
        printf("%c",217);
        printf(CONSOLE_ESC(5;52H));
        printf("%c",192);
        for (int i = 0; i < 27; i++) {
            printf("%c",196);
        }
    } else if (selected == 3) {
        printf(CONSOLE_ESC(3;54H));
        printf("%c",218);
        for (int i = 0; i < 22; i++) {
            printf("%c",196);
        }
        printf("%c",191);
        printf(CONSOLE_ESC(4;54H));
        printf("%c",179);
        printf(CONSOLE_ESC(4;77H));
        printf("%c",179);
        printf(CONSOLE_ESC(5;2H));
        for (int i = 0; i < 52; i++) {
            printf("%c",196);
        }
        printf("%c",217);
        printf(CONSOLE_ESC(5;77H));
        printf("%c",192);
        printf("%c",196);
        printf("%c",196);
    }
    printf(CONSOLE_ESC(4;14H));
    printf("%s","Push");
    printf(CONSOLE_ESC(4;39H));
    printf("%s","Pull");
    printf(CONSOLE_ESC(4;63H));
    printf("%s","Config");
}
void drawAppMenu() {
    printf(CONSOLE_ESC(48;5;237m));
    printf(CONSOLE_ESC(6;5H));
    printf("%c",201);
    for (int i = 0; i < 70; i++) {
        printf("%c",205);
    }
    printf("%c",187);
    printf(CONSOLE_ESC(7;5H));
    for (int i = 0; i < 33; i++) {
        printf("%c\n",186);
        printf(CONSOLE_ESC(4C));
    }
    printf("%c",200);
    for (int i = 0; i < 70; i++) {
        printf("%c",205);
    }
    printf("%c",188);
    printf(CONSOLE_ESC(7;1H));
    for (int i = 0; i < 33; i++) {
        printf(CONSOLE_ESC(75C));
        printf("%c\n",186);
    }
    printf(CONSOLE_ESC(7;6H));
    for (int i = 0; i < 33; i++) {
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
void drawTempZipWarning() {
    printf(CONSOLE_ESC(48;5;237m));
    printf(CONSOLE_ESC(20;30H));
    printf("%c",201);
    for (int i = 0; i < 20; i++) {
        printf("%c",205);
    }
    printf("%c",187);
    printf(CONSOLE_ESC(21;30H));
    for (int i = 0; i < 5; i++) {
        printf("%c\n",186);
        printf(CONSOLE_ESC(29C));
    }
    printf("%c",200);
    for (int i = 0; i < 20; i++) {
        printf("%c",205);
    }
    printf("%c",188);
    printf(CONSOLE_ESC(21;1H));
    for (int i = 0; i < 5; i++) {
        printf(CONSOLE_ESC(50C));
        printf("%c\n",186);
    }
    printf(CONSOLE_ESC(21;31H));
    for (int i = 0; i < 5; i++) {
        printf("                    \n");
        printf(CONSOLE_ESC(30C));
    }
    printf(CONSOLE_ESC(48;5;0m));
}