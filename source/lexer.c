#include <lexer.h>
#include <stdlib.h>
#include <assert.h>
#include <error.h>

static bool is_whitespace(char c) {
    return c == '\n' || c == '\r' || c == '\t' || c == ' ';
}

static bool is_number(char c) {
    return '0' <= c && c <= '9';
}

static bool is_valid_letter(char c) {
    if (c == '_') {
        return true;
    }

    // Convert from upper-case to lower-case.
    c |= (1 << 5);

    if ('a' <= c && c <= 'z') {
        return true;
    }

    return false;
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

    return lexer->cursor[0];
}

static void advance_lexer_with(Lexer* lexer, u32 count) {
    while (count--) {
        advance_lexer(lexer);
    }
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
    lexer->column += skip_count;
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
        case '[' : {
            skip_punctuation(lexer, 1, token, TOKEN_OPEN_SQUARE);
            break;
        }
        case ']' : {
            skip_punctuation(lexer, 1, token, TOKEN_CLOSE_SQUARE);
            break;
        }
        case '+' : {
            skip_punctuation(lexer, 1, token, TOKEN_PLUS);
            break;
        }
        case '-' : {
            if (next == '>') {
                skip_punctuation(lexer, 2, token, TOKEN_ARROW);
            }
            else {
                skip_punctuation(lexer, 1, token, TOKEN_MINUS);
            }
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
            else {
                skip_punctuation(lexer, 1, token, TOKEN_ASSIGN);
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
        case ':' : {
            if (next == ':') {
                skip_punctuation(lexer, 2, token, TOKEN_DOUBLE_COLON);
            }
            else {
                skip_punctuation(lexer, 1, token, TOKEN_COLON);
            }
            break;
        }
        case ',' : {
            skip_punctuation(lexer, 1, token, TOKEN_COMMA);
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

static void parse_identifier(Lexer* lexer, Token* token) {
    token->name.text = lexer->cursor;
    token->kind = TOKEN_IDENTIFIER;

    char c = lexer->cursor[0];
    while (is_number(c) || is_valid_letter(c)) {
        c = advance_lexer(lexer);
    }

    token->name.size = lexer->cursor - token->name.text;
}

static void parse_comment(Lexer* lexer, Token* token) {
    token->kind = TOKEN_COMMENT;

    // We allready know that the two first characters are correct. 
    advance_lexer_with(lexer, 2);
    token->name.text = lexer->cursor;

    if (lexer->cursor[0] == '(') {
        u32 nesting_level = 1;
       
        while (1) {
            if (lexer->cursor[0] == '/' && lexer->cursor[1] == '/') {
                char c = lexer->cursor[2];
                if (c == '(') {
                    nesting_level++;
                } 
                else if (c == ')') {
                    nesting_level--;
                }

                if (nesting_level == 0) {
                    advance_lexer_with(lexer, 3);
                    break;
                }
            }

            advance_lexer(lexer);
        }
    }
    else {
        while (lexer->cursor[0] != '\r' && lexer->cursor[0] != '\n') {
            advance_lexer(lexer);
        }
    }

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
    else if (c == '/' && lexer->cursor[1] == '/') {
        parse_comment(lexer, token);
    }
    else if (is_valid_letter(c)) {
        parse_identifier(lexer, token);
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
    lexer->line   = 1;

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

static const char* token_kind[] = {
    "none",
    "end of file",
    "a number",
    "an identifier",
    "a comment",
    "+", 
    "-", 
    "*",
    "/",
    "==",
    "<",
    "<=",
    ">",
    ">=",
    "=",
    "(",
    ")",
    "{",
    "}",
    "[",
    "]",
    ";",
    ":",
    "::"
    "->",
    ","
};

Token* expect_token(Lexer* lexer, TokenKind kind) {
    Token* token = next_token(lexer);
    if (token->kind != kind) {
        assert(token->kind < TOKEN_KIND_COUNT);
        assert(kind < TOKEN_KIND_COUNT);

        error_token(token, "expecting %s but got %s", token_kind[kind], token_kind[token->kind]);
    }

    return token;
}

Token* skip_token(Lexer* lexer, TokenKind kind) {
    Token* token = current_token(lexer);
    if (token->kind != kind) {
        assert(token->kind < TOKEN_KIND_COUNT);
        assert(kind < TOKEN_KIND_COUNT);

        error_token(token, "expecting %s but got %s", token_kind[kind], token_kind[token->kind]);
    }

    return next_token(lexer);
}

static const char* keywords[] = {
    "func",
    "u64",
    "u32",
    "u16",
    "u8",
    "s64",
    "s32",
    "s16",
    "s8",
    "char",
    "return"
};

bool is_keyword(Token* token, KeywordKind kind) {
    if (kind >= KEYWORD_KIND_COUNT) {
        printf("Keyword not handled\n");
        exit(1);
    }

    const char* a = token->name.text;
    const char* b = keywords[kind];

    for (u32 i = 0; i < token->name.size; i++, a++, b++) {
        if (*b == 0 || *a != *b) {
            return false;
        }
    }

    if (*b) {
        return false;
    }

    return true;
}
