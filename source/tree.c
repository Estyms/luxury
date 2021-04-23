#include <tree.h>
#include <stdlib.h>

void* new_statement(StatementKind kind) {
    Statement* statement = calloc(1, sizeof(Statement));
    statement->kind = kind;
    return statement;
}

void* new_compound_statement() {
    Compound* compound = new_statement(STATEMENT_COMPOUND);
    list_init(&compound->statements);
    return compound;
}

void* new_expression(ExpressionKind kind) {
    Expression* expression = calloc(1, sizeof(Expression));
    expression->kind = kind;
    return expression;
}

void* new_binary() {
    Binary* binary = new_expression(EXPRESSION_BINARY);
    return binary;
}