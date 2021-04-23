#ifndef LEXER_H
#define LEXER_H

#include <types.h>
#include <str.h>

#define TOKEN_PEEK_COUNT 10
#define TOKEN_UNDO_COUNT 10
#define TOKEN_BUFFER_SIZE (TOKEN_PEEK_COUNT + TOKEN_UNDO_COUNT + 2)

enum TokenKind {
    TOKEN_NONE,
    TOKEN_END_OF_FILE,
    
    TOKEN_NUMBER,
    
    TOKEN_PLUS,              // +
    TOKEN_MINUS,             // -
    TOKEN_MULTIPLICATION,    // *
    TOKEN_DIVISION,          // /

    TOKEN_EQUAL,             // ==
    TOKEN_LESS,              // <
    TOKEN_LESS_EQUAL,        // <=
    TOKEN_GREATER,           // > 
    TOKEN_GREATER_EQUAL,     // >=

    TOKEN_OPEN_PARENTHESIS,  // (
    TOKEN_CLOSE_PARENTHESIS, // )
    TOKEN_OPEN_CURLY,        // {
    TOKEN_CLOSE_CURLY,       // }

    TOKEN_SEMICOLON,         // ;

    COUNT_TOKEN
};

struct Token {
    TokenKind kind;
    String name;

    Lexer* lexer;

    u32 line;
    u32 column;

    u64 number;
};

struct Lexer {
    // Must be terminated with a zero.
    String file;
    String file_name;

    char* cursor;

    u32 line;
    u32 column;

    struct {
        Token token;
        bool is_valid;
    } tokens[TOKEN_BUFFER_SIZE];

    u32 current_index;
    u32 buffer_index;
};

Lexer* new_lexer(String* file, String* file_name);

Token* next_token(Lexer* lexer);
Token* peek_token(Lexer* lexer, u32 count);
Token* undo_next_token(Lexer* lexer);
Token* peek_next(Lexer* lexer);
Token* current_token(Lexer* lexer);
Token* consume_token(Lexer* lexer);
Token* expect_token(Lexer* lexer, TokenKind kind);
Token* skip_token(Lexer* lexer, TokenKind kind);

#endif