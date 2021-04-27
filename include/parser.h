#ifndef PARSER_H
#define PARSER_H

#include <types.h>
#include <tree.h>
#include <lexer.h>

struct Parser {
    Lexer* lexer;

    Scope* current_scope;
    StructScope* current_struct_scope;
};

Program* parser_program(Parser* parser);
Parser* new_parser(Lexer* lexer);

#endif


