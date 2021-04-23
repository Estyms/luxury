#ifndef ERR_H
#define ERR_H

#include <types.h>
#include <lexer.h>

void error_token(Token* token, const char* message);

#endif