#ifndef LEXER_H
#define LEXER_H

#include <types.h>
#include <string.h>

#define TOKEN_PEEK_COUNT 10
#define TOKEN_UNDO_COUNT 10
#define TOKEN_BUFFER_SIZE (TOKEN_PEEK_COUNT + TOKEN_UNDO_COUNT + 2)

enum TokenKind {
    TOKEN_NONE,
    TOKEN_END_OF_FILE,
    
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_COMMENT,
    
    TOKEN_PLUS,              // +
    TOKEN_MINUS,             // -
    TOKEN_MULTIPLICATION,    // *
    TOKEN_DIVISION,          // /

    TOKEN_EQUAL,             // ==
    TOKEN_LESS,              // <
    TOKEN_LESS_EQUAL,        // <=
    TOKEN_GREATER,           // > 
    TOKEN_GREATER_EQUAL,     // >=
    TOKEN_ASSIGN,            // =

    TOKEN_OPEN_PARENTHESIS,  // (
    TOKEN_CLOSE_PARENTHESIS, // )
    TOKEN_OPEN_CURLY,        // {
    TOKEN_CLOSE_CURLY,       // }
    TOKEN_OPEN_SQUARE,       // [
    TOKEN_CLOSE_SQUARE,      // ]

    TOKEN_SEMICOLON,         // ;
    TOKEN_COLON,             // :
    TOKEN_DOUBLE_COLON,      // ::
    TOKEN_ARROW,             // ->
    TOKEN_COMMA,             // ,

    TOKEN_KIND_COUNT
};

enum KeywordKind {
    KEYWORD_FUNC,
    KEYWORD_U64,
    KEYWORD_U32,
    KEYWORD_U16,
    KEYWORD_U8,
    KEYWORD_S64,
    KEYWORD_S32,
    KEYWORD_S16,
    KEYWORD_S8,
    KEYWORD_CHAR,
    KEYWORD_RETURN,

    KEYWORD_KIND_COUNT
};

struct Token {
    TokenKind kind;
    String name;

    // The reason for this is that we get the start location of the source file in case we sometimes
    // need to backtrack.
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


bool is_keyword(Token* token, KeywordKind kind);

#endif