#ifndef TREE_H
#define TREE_H

#include <types.h>
#include <lexer.h>
#include <list.h>

enum ExpressionKind {
    EXPRESSION_PRIMARY,
    EXPRESSION_UNARY,
    EXPRESSION_BINARY
};

enum PrimaryKind {
    PRIMARY_NUMBER,
    PRIMARY_KIND_COUNT
};

enum UnaryKind {
    UNARY_KIND_COUNT,
};

enum BinaryKind {
    BINARY_PLUS = 1,
    BINARY_MINUS,
    BINARY_MULTIPLICATION,
    BINARY_DIVISION,
    BINARY_EQUAL,
    BINARY_LESS,
    BINARY_LESS_EQUAL,
    BINARY_GREATER,
    BINARY_GREATER_EQUAL,
    BINARY_KIND_COUNT
};

enum StatementKind {
    STATEMENT_EXPRESSION,
    STATEMENT_COMPOUND,
    STATEMENT_KIND_COUNT
};

struct Primary {
    PrimaryKind kind;
    Token* token;

    union {
        u64 number;
    };
};

struct Unary {
    UnaryKind kind;
    Token* operator;
    Expression* operand;
};

struct Binary {
    BinaryKind kind;
    Token* operator;
    Expression* left;
    Expression* right;
};

struct Expression {
    union {
        Binary  binary;
        Unary   unary;
        Primary primary;
    };

    ExpressionKind kind;
};

struct Compound {
    List statements;
};

struct Statement {
    union {
        Compound      compound;
        Expression*   expression;
    };

    StatementKind kind;
    ListNode list_node;
};

void* new_statement(StatementKind kind);
void* new_compound_statement();
void* new_expression(ExpressionKind kind);
void* new_binary();

#endif