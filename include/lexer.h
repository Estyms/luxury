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
    TOKEN_STRING,
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
    TOKEN_NOT_EQUAL,         // !=

    TOKEN_OPEN_PARENTHESIS,  // (
    TOKEN_CLOSE_PARENTHESIS, // )
    TOKEN_OPEN_CURLY,        // {
    TOKEN_CLOSE_CURLY,       // }
    TOKEN_OPEN_SQUARE,       // [
    TOKEN_CLOSE_SQUARE,      // ]

    TOKEN_DOT,               // .
    TOKEN_DOUBLE_DOT,        // ..
    TOKEN_SEMICOLON,         // ;
    TOKEN_COLON,             // :
    TOKEN_DOUBLE_COLON,      // ::
    TOKEN_ARROW,             // ->
    TOKEN_COMMA,             // ,

    TOKEN_BITWISE_XOR,       // ^
    TOKEN_BITWISE_AND,       // &
    TOKEN_AT,                // @

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
    KEYWORD_FOR,
    KEYWORD_WHILE,
    KEYWORD_IF,
    KEYWORD_ELSE,
    KEYWORD_IN,
    KEYWORD_STRUCT,
    KEYWORD_UNION,

    KEYWORD_KIND_COUNT
};

struct Token {
    TokenKind kind;

    Lexer* lexer;
    String name;
    u32    line;
    u32    column;

    u64 number;
};

struct Lexer {
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

// Just returns the next token.
Token* next_token(Lexer* lexer);

// Returns the token 'count' ahead, but does not move the cursor.
Token* peek_token(Lexer* lexer, u32 count);

// Moves the cursor one step back.
Token* undo_next_token(Lexer* lexer);

// Returns the next token, but does not move the cursor.
Token* peek_next(Lexer* lexer);

// Returns the current token.
Token* current_token(Lexer* lexer);

// Returns the current token, and moves the cursor to the next token.
Token* consume_token(Lexer* lexer);

// Returns the next token if it matches the 'kind', otherwise it signals an error.
Token* expect_token(Lexer* lexer, TokenKind kind);

// Returns next token if the current token matches the 'kind', otherwise it signals an error.
Token* skip_token(Lexer* lexer, TokenKind kind);

bool is_keyword(Token* token, KeywordKind kind);

// Return the next toke if the current token is the given keyword, otherwise it signals an error.
Token* skip_keyword(Lexer* lexer, KeywordKind kind);

#endif