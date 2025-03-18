#include <stdio.h>
#include <switch.h>

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