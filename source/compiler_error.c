#include <compiler_error.h>
#include <stdlib.h>
#include <stdio.h>

const u32 LINE_COUNT = 3;

void error_token(Token* token, const char* message) {
    const char* start = token->lexer->file.text;
    const char* current = token->name.text;

    u32 i;
    for (i = 0; i < LINE_COUNT; i++) {
        while (current != start && *current != '\n') {
            current--;
        }

        if (*current == '\n') {
            current--;
        }

        if (current == start) {
            break;
        }
    }

    if (current != start) {
        current++;
        if (*current == '\n') {
            current++;
        }
    }

    u32 line = token->line - i;
    printf("Error: %s\n", message);
    for (u32 j = 0; j < (i + 1); j++) {
        printf(" %3d | ", line++);

        while (*current && *current != '\n' && *current != '\r') {
            printf("%c", *current++);
        }

        if (*current == '\r') {
            current++;
        }

        if (*current == '\n') {
            current++;
        }

        printf("\n");
    }

    printf("       ");
    for (u32 i = 0; i < token->column; i++) {
        printf(" ");
    }
    for (u32 i = 0; i < token->name.size; i++) {
        printf("^");
    }
    printf("\n");
    exit(0);
}
