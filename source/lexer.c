#include <lexer.h>
#include <stdlib.h>
#include <assert.h>
#include <compiler_error.h>

static bool is_whitespace(char c) {
    return c == '\n' || c == '\r' || c == '\t' || c == ' ';
}

static bool is_number(char c) {
    return '0' <= c && c <= '9';
}

static char advance_lexer(Lexer* lexer) {
    char c = lexer->cursor[0];
    if (c == 0) {
        return;
    }

    lexer->cursor++;
    if (c == '\r') {
        if (lexer->cursor[0] == '\n') {
            lexer->cursor++;
        }

        c = '\n';
    }

    if (c == '\n') {
        lexer->line++;
        lexer->column = 0;
    }
    else {
        lexer->column++;
    }

    return c;
}

static void skip_whitespaces(Lexer* lexer) {
    while (is_whitespace(lexer->cursor[0]) && lexer->cursor[0]) {
        advance_lexer(lexer);
    }
}

static void skip_punctuation(Lexer* lexer, u32 skip_count, Token* token, TokenKind kind) {
    token->name.text = lexer->cursor;
    token->name.size = skip_count;
    token->kind      = kind;

    lexer->cursor += skip_count;
}

static void parse_puctuation(Lexer* lexer, Token* token) {
    char next = lexer->cursor[1];
    switch (lexer->cursor[0]) {
        case '(' : {
            skip_punctuation(lexer, 1, token, TOKEN_OPEN_PARENTHESIS);
            break;
        }
        case ')' : {
            skip_punctuation(lexer, 1, token, TOKEN_CLOSE_PARENTHESIS);
            break;
        }
        case '{' : {
            skip_punctuation(lexer, 1, token, TOKEN_OPEN_CURLY);
            break;
        }
        case '}' : {
            skip_punctuation(lexer, 1, token, TOKEN_CLOSE_CURLY);
            break;
        }
        case '+' : {
            skip_punctuation(lexer, 1, token, TOKEN_PLUS);
            break;
        }
        case '-' : {
            skip_punctuation(lexer, 1, token, TOKEN_MINUS);
            break;
        }
        case '*' : {
            skip_punctuation(lexer, 1, token, TOKEN_MULTIPLICATION);
            break;
        }
        case '/' : {
            skip_punctuation(lexer, 1, token, TOKEN_DIVISION);
            break;
        }
        case '=' : {
            if (next == '=') {
                skip_punctuation(lexer, 2, token, TOKEN_EQUAL);
            }
            break;
        }
        case '<' : {
            if (next == '=') {
                skip_punctuation(lexer, 2, token, TOKEN_LESS_EQUAL);
            }
            else {
                skip_punctuation(lexer, 1, token, TOKEN_LESS);
            }
            break;
        }
        case '>' : {
            if (next == '=') {
                skip_punctuation(lexer, 2, token, TOKEN_GREATER_EQUAL);
            }
            else {
                skip_punctuation(lexer, 1, token, TOKEN_GREATER);
            }
            break;
        }
        case ';' : {
            skip_punctuation(lexer, 1, token, TOKEN_SEMICOLON);
            break;
        }
    }
}

static s8 char_to_number(char c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    }

    // Convert from upper-case to lower-case.
    c |= (1 << 5);

    if ('a' <= c && c <= 'f') {
        return c - 'a';
    }

    return -1;
}

static void parse_number(Lexer* lexer, Token* token) {
    u32 base = 10;
    token->name.text = lexer->cursor;
    token->kind = TOKEN_NUMBER;

    if (lexer->cursor[0] == '0') {
        if (lexer->cursor[1] == 'x') {
            base = 16;
        }
        else if (lexer->cursor[1] == 'b') {
            base = 2;
        }
        else if (lexer->cursor[1] == 'o') {
            base = 8;
        }

        if (base == 10) {
            exit(3);
        }

        advance_lexer(lexer);
        advance_lexer(lexer);
    }
    
    u64 number = 0;

    while (1) {
        s8 tmp = char_to_number(lexer->cursor[0]);
        if (tmp == -1) {
            break;
        }

        if (tmp > base) {
            exit(5);
        }

        number = number * base + tmp;
        advance_lexer(lexer);
    }

    token->number = number;
    token->name.size = lexer->cursor - token->name.text;
}

static void process_next_token(Lexer* lexer, Token* token) {
    assert(token->lexer);
    token->kind = TOKEN_NONE;

    skip_whitespaces(lexer);

    token->line   = lexer->line;
    token->column = lexer->column;

    char c = lexer->cursor[0];
    if (c == 0) {
        token->kind = TOKEN_END_OF_FILE;
        return;
    }

    if (is_number(c)) {
        parse_number(lexer, token);
    }
    else {
        parse_puctuation(lexer, token);
    }
    
    if (token->kind == TOKEN_NONE) {
        printf("Go fix the lexer\n");
        exit(1);
    }
}

Lexer* new_lexer(String* file, String* file_name) {
    Lexer* lexer = calloc(1, sizeof(Lexer));

    lexer->file.size = file->size;
    lexer->file.text = file->text;

    lexer->file_name.size = file_name->size;
    lexer->file_name.text = file_name->text;

    lexer->column = 0;
    lexer->line   = 0;

    lexer->cursor = lexer->file.text;

    for (u32 i = 0; i < TOKEN_BUFFER_SIZE; i++) {
        lexer->tokens[i].is_valid = false;
        lexer->tokens[i].token.lexer = lexer;
    }

    lexer->current_index = 0;
    lexer->buffer_index  = 0;

    return lexer;
}

static void increment_index(u32* index) {
    *index += 1;
    if (*index == TOKEN_BUFFER_SIZE) {
        *index = 0;
    }
}

static u32 get_undo_distance(Lexer* lexer) {
    if (lexer->current_index >= lexer->buffer_index) {
        return lexer->current_index - lexer->buffer_index;
    }
    else {
        return lexer->current_index + TOKEN_BUFFER_SIZE - lexer->buffer_index;
    }
}

Token* next_token(Lexer* lexer) {
    if (get_undo_distance(lexer) > TOKEN_UNDO_COUNT) {
        lexer->tokens[lexer->buffer_index].is_valid = false;
        increment_index(&lexer->buffer_index);
    }

    increment_index(&lexer->current_index);

    Token* token = &lexer->tokens[lexer->current_index].token;
    if (lexer->tokens[lexer->current_index].is_valid == false) {
        lexer->tokens[lexer->current_index].is_valid = true;
        process_next_token(lexer, token);
    }

    return token;
}

Token* peek_token(Lexer* lexer, u32 count) {
    if (count > TOKEN_PEEK_COUNT) {
        exit(3);
    }

    u32 index = lexer->current_index;
    for (u32 i = 0; i < count; i++) {
        increment_index(&index);

        if (lexer->tokens[index].is_valid == false) {
            lexer->tokens[index].is_valid = true;
            process_next_token(lexer, &lexer->tokens[index].token);
        }
    }

    return &lexer->tokens[index].token;
}

Token* undo_next_token(Lexer* lexer) {
    if (get_undo_distance(lexer) > 1) {
        if (lexer->current_index) {
            lexer->current_index -= 1;
        }
        else {
            lexer->current_index = TOKEN_BUFFER_SIZE - 1;
        }

        assert(lexer->tokens[lexer->current_index].is_valid == true);
        return &lexer->tokens[lexer->current_index].token;
    }

    exit(21);
}

Token* peek_next(Lexer* lexer) {
    return peek_token(lexer, 1);
}

Token* current_token(Lexer* lexer) {
    assert(lexer->current_index != lexer->buffer_index);
    return peek_token(lexer, 0);
}

Token* consume_token(Lexer* lexer) {
    Token* token = current_token(lexer);
    next_token(lexer);
    return token;
}

Token* expect_token(Lexer* lexer, TokenKind kind) {

}

Token* skip_token(Lexer* lexer, TokenKind kind) {
    Token* token = current_token(lexer);
    if (token->kind != kind) {
        error_token(token, "skip token failed");
    }

    next_token(lexer);
}
