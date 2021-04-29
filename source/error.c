#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

// This defines the line history which should be printed when having an error.
const u32 LINE_COUNT = 3;

#define NORMAL  "\x1B[0m"
#define RED     "\x1B[31m"

// This will print an error message based on the token. The token will contain a pointer to the 
// lexer for additional information.The format will be the following:
// 
//   3 | data := 3;
//   4 | 
//   5 | main : func () -> u2 {
//                         ^^
//                         message
void error_token(Token* token, const char* message, ...) {
    const char* start   = token->lexer->file.text;
    const char* current = token->name.text;

    // Trace back 'LINE_COUNT' number of lines.
    u32 i = 0;
    while (i < LINE_COUNT) {
        i++;
        while (current != start && *current != '\n') {
            current--;
        }

        if (current == start) {
            break;
        }
        
        if (i == LINE_COUNT - 1) {
            current++;
        } 
        else {
            current--;
        }
    }


    printf(RED "Error: \n" NORMAL);

    u32 line = token->line - i + 1;

    for (u32 j = 0; j < i; j++) {
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

    printf("\n       ");

    for (u32 i = 0; i < token->column; i++) {
        printf(" ");
    }

    // The error message goes after the file trace. 
    static char buffer[1024];

    va_list arg;
    va_start(arg, message);
    u32 size = vsnprintf(buffer, 1024, message, arg);
    va_end(arg);
    
    printf("%.*s\n", size, buffer);

    printf("\n");
    exit(1);
}
