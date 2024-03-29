#ifndef TREE_H
#define TREE_H

#include <types.h>
#include <lexer.h>
#include <list.h>

enum ExpressionKind {
    EXPRESSION_PRIMARY = 1,
    EXPRESSION_UNARY,
    EXPRESSION_BINARY,
    EXPRESSION_CALL,
    EXPRESSION_DOT,
};

enum PrimaryKind {
    PRIMARY_NUMBER = 1,
    PRIMARY_IDENTIFIER,
    PRIMARY_STRING,
    PRIMARY_KIND_COUNT
};

enum UnaryKind {
    UNARY_DEREF = 1,
    UNARY_ADDRESS_OF,
    UNARY_KIND_COUNT,
};

enum BinaryKind {
    BINARY_PLUS = 1,
    BINARY_MINUS,
    BINARY_MULTIPLICATION,
    BINARY_DIVISION,
    BINARY_EQUAL,
    BINARY_NOT_EQUAL,
    BINARY_LESS,
    BINARY_LESS_EQUAL,
    BINARY_GREATER,
    BINARY_GREATER_EQUAL,
    BINARY_ASSIGN,
    BINARY_KIND_COUNT
};

enum StatementKind {
    STATEMENT_EXPRESSION = 1,
    STATEMENT_COMPOUND,
    STATEMENT_COMMENT,
    STATEMENT_RETURN,
    STATEMENT_LOOP,
    STATEMENT_CONDITIONAL,
    STATEMENT_KIND_COUNT
};

enum TypeKind {
    TYPE_BASIC = 1,
    TYPE_POINTER,
    TYPE_INFERRED,
    TYPE_UNKNOWN,
    TYPE_STRUCT,
    TYPE_VOID,
    TYPE_KIND_COUNT
};

enum DeclarationKind {
    DECLARATION_VARIABLE = 1,
    DECLARATION_FUNCTION,
    DECLARATION_TYPE,
};

struct Primary {
    PrimaryKind kind;
    Token* token;

    union {
        struct {
            String name;
            Declaration* declaration;
        };
        u64 number;
        String string;
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

struct Call {
    Token* token;
    Expression* expression;

    List arguments;   // Expressions.
};

struct Dot {
    Token* dot_token;
    Token* member;
    u32 offset;
    Expression* expression;
};

struct Expression {
    union {
        Binary  binary;
        Unary   unary;
        Primary primary;
        Call    call;
        Dot     dot;
    };

    ExpressionKind kind;
    Type* type;
    ListNode list_node; // Do we need this?
};

struct Compound {
    List statements;
    Scope* scope;
};

struct Comment {
    Token* token;
};

struct ReturnStatement {
    Expression* return_expression;
};

struct Loop {
    Statement* body;
    Statement* post_statement;
    Expression* condition;
    Statement* init_statement;
};

struct Conditional {
    Expression* condition;
    Statement* true_body;
    Statement* false_body;
};

struct Statement {
    union {
        Compound        compound;
        Comment         comment;
        Expression*     expression;
        ReturnStatement Return;
        Loop            loop;
        Conditional     conditional;
    };

    StatementKind kind;
    ListNode list_node;
};

struct PointerType {
    Type* pointer_to;
    u32 count;  // In case of array.
};

struct BasicType {
    bool is_signed;
};

struct UnknownType {
    Token* token;
};

struct StructScope {
    StructScope* parent;
    List members;
    bool typing_complete;
};

struct StructType {
    // List of struct members.
    List members;

    bool is_struct;
    StructScope* scope;
};

struct StructMember {
    ListNode list_node;
    ListNode scope_node;

    bool is_anonymous;

    Type* type;
    String name;

    Token* token;

    u32 offset;
};

struct Type {
    union {
        PointerType  pointer;
        BasicType    basic;
        UnknownType  unknown;
        StructType   Struct;
    };

    TypeKind kind;

    u32 size;
    u32 alignment;
};

struct Variable {
    s32 offset;
};

struct Function {
    Type* return_type;
    
    Statement* body;
    String assembly_body;

    Scope* function_scope;
    bool assembly_function;
};

struct Declaration {
    union {
        Variable variable;  
        Function function;
    };

    bool is_global;

    DeclarationKind kind;
    Token* name_token;

    // Delcaration mapping.
    String name;
    Type* type;

    ListNode list_node;
};

struct Scope {
    // These list various declarations.
    List variables;
    List functions;
    List types;

    ListNode list_node;

    // This is for iterating through all the scopes, and for recursivly looking 
    // up variables.
    Scope* parent;
    List child_scopes;
};

struct CodeUnit {
    ListNode list_node;
    String file_name;

    Scope* global_scope;
};

struct Program {
    List code_units;
};

Program* new_program();
CodeUnit* new_code_unit();
Scope* new_scope();
Declaration* new_declaration();

void* new_type(TypeKind kind);
void* new_pointer();

void* new_statement(StatementKind kind);
void* new_compound_statement();
void* new_expression(ExpressionKind kind);
void* new_binary(BinaryKind kind);
void* new_primary(PrimaryKind kind);
Call* new_call();
Unary* new_unary(UnaryKind kind);

StructType* new_struct();
StructMember* new_struct_member();
StructScope* new_struct_scope();


bool is_deref(Expression* expression);
bool is_variable(Expression* expression);
bool is_inferred(Expression* expression);

#endif