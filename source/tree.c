#include <tree.h>
#include <stdlib.h>

Scope* new_scope() {
    Scope* scope = calloc(1, sizeof(Scope));

    list_init(&scope->functions);
    list_init(&scope->variables);
    list_init(&scope->types);
    list_init(&scope->child_scopes);

    return scope;
}

Declaration* new_declaration() {
    Declaration* declaration = calloc(1, sizeof(Declaration));
    return declaration;
}

Program* new_program() {
    Program* program = calloc(1, sizeof(Program));

    list_init(&program->code_units);
    return program;
}

CodeUnit* new_code_unit() {
    CodeUnit* unit = calloc(1, sizeof(CodeUnit));
    return unit;
}

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

void* new_binary(BinaryKind kind) {
    Binary* binary = new_expression(EXPRESSION_BINARY);
    binary->kind = kind;
    return binary;
}

void* new_primary(PrimaryKind kind) {
    Primary* primary = new_expression(EXPRESSION_PRIMARY);
    primary->kind = kind;
    return primary;
}

void* new_type(TypeKind kind) {
    Type* type = calloc(1, sizeof(Type));
    type->kind = kind;
    return type;
}

void* new_pointer() {
    Type* type = new_type(TYPE_POINTER);

    type->size      = 8;
    type->alignment = 8;

    return (void *)type;
}

Call* new_call() {
    Call* call = new_expression(EXPRESSION_CALL);
    list_init(&call->arguments);
    return call;
}

Unary* new_unary(UnaryKind kind) {
    Unary* unary = new_expression(EXPRESSION_UNARY);
    unary->kind = kind;
    return unary;
}


bool is_deref(Expression* expression) {
    return expression->kind == EXPRESSION_UNARY && expression->unary.kind == UNARY_DEREF;
}

bool is_variable(Expression* expression) {
    return expression->kind == EXPRESSION_PRIMARY && expression->primary.kind == PRIMARY_IDENTIFIER;
}
