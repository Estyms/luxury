#include <parser.h>
#include <stdlib.h>
#include <list.h>
#include <assert.h>


static Statement* parse_compound_statement(Parser* parser);
static Expression* parse_expression(Parser* parser, s8 priority);
static Expression* parse_unary_expression(Parser* parser);
static Expression* parse_primary_expression(Parser* parser);
static Statement* parse_expression_statement(Parser* parser);

static void print_token(Token* token) {
    printf("%.*s\n", token->name.size, token->name.text);
}

static Token* copy_token(Token* token) {
    Token* new = calloc(1, sizeof(Token));

    u8* source = (u8 *)token;
    u8* dest   = (u8 *)new;

    for (u32 i = 0; i < sizeof(Token); i++) {
        *dest++ = *source++;
    }

    return new;
}

#define EXPRESSION_NO_PRIORITY   0
#define EXPRESSION_INIT_PRIORITY -1

static BinaryKind token_to_binary_kind(TokenKind kind) {
    switch (kind) {
        case TOKEN_EQUAL          : return BINARY_EQUAL;
        case TOKEN_GREATER        : return BINARY_GREATER;
        case TOKEN_GREATER_EQUAL  : return BINARY_GREATER_EQUAL;
        case TOKEN_LESS           : return BINARY_LESS;
        case TOKEN_LESS_EQUAL     : return BINARY_LESS_EQUAL;
        case TOKEN_MINUS          : return BINARY_MINUS;
        case TOKEN_PLUS           : return BINARY_PLUS;
        case TOKEN_DIVISION       : return BINARY_DIVISION;
        case TOKEN_MULTIPLICATION : return BINARY_MULTIPLICATION;
    }

    return 0;
};

static s8 get_binary_priority(BinaryKind kind) {
    switch (kind) {
        case BINARY_MULTIPLICATION:
        case BINARY_DIVISION:
            return 30;
        case BINARY_PLUS:
        case BINARY_MINUS: 
            return 24;
        case BINARY_LESS:
        case BINARY_LESS_EQUAL:
        case BINARY_GREATER:
        case BINARY_GREATER_EQUAL:
            return 20;
        case BINARY_EQUAL:
            return 19;
    }

    return EXPRESSION_NO_PRIORITY;
}

static Expression* parse_expression(Parser* parser, s8 priority) {
    assert(parser);
    Expression* left = parse_unary_expression(parser);

    while (1) {
        Lexer* lexer = parser->lexer;
        Token* token = current_token(lexer);

        BinaryKind kind = token_to_binary_kind(token->kind);
        s8 new_priority = get_binary_priority(kind);

        if (new_priority == EXPRESSION_NO_PRIORITY || new_priority <= priority) {
            return left;
        }

        next_token(lexer);  // Skip the operator.
        
        Binary* binary = new_binary();
        binary->kind     = kind;
        binary->operator = copy_token(token);
        binary->left     = left;
        binary->right    = parse_expression(parser, new_priority);

        left = (Expression *)binary;
    }
}

static Expression* parse_unary_expression(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

    if (token->kind == TOKEN_OPEN_PARENTHESIS) {
        next_token(lexer);
        Expression* expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
        skip_token(lexer, TOKEN_CLOSE_PARENTHESIS);

        return expression;
    }
    
    return parse_primary_expression(parser);
}

static Expression* parse_primary_expression(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    Primary* primary = new_expression(EXPRESSION_PRIMARY);
    primary->token = copy_token(token);

    switch (token->kind) {
        case TOKEN_NUMBER : {
            primary->number = token->number;
            break;
        }
    }

    return (Expression *)primary;
}

static Statement* parse_expression_statement(Parser* parser) {
    Statement* statement = new_statement(STATEMENT_EXPRESSION);
    statement->expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
    skip_token(parser->lexer, TOKEN_SEMICOLON);

    return statement;
}

Statement* parse_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

    if (token->kind == TOKEN_OPEN_CURLY) {
        return parse_compound_statement(parser);
    }

    return parse_expression_statement(parser);
}

static Statement* parse_compound_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;

    skip_token(lexer, TOKEN_OPEN_CURLY);

    Compound* compound = new_compound_statement();
    Token* token = current_token(lexer);

    while (token->kind != TOKEN_CLOSE_CURLY && token->kind != TOKEN_END_OF_FILE) {
        Statement* statement = parse_statement(parser);
        list_add_last(&statement->list_node, &compound->statements);

        token = current_token(lexer);
    }

    skip_token(lexer, TOKEN_CLOSE_CURLY);
    return (Statement *)compound;
}

Parser* new_parser(Lexer* lexer) {
    Parser* parser = calloc(1, sizeof(Parser));

    parser->lexer = lexer;

    next_token(lexer);
    return parser;
}