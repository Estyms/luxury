#ifndef ERROR_H
#define ERROR_H

#include <types.h>
#include <lexer.h>
#include <stdarg.h>

void error_token(Token* token, const char* message, ...);

#endif