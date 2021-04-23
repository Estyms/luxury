#ifndef PARSER_H
#define PARSER_H

#include <types.h>
#include <tree.h>
#include <lexer.h>

struct Parser {
    Lexer* lexer;
};

Statement* parse_statement(Parser* parser);
Parser* new_parser(Lexer* lexer);

#endif


