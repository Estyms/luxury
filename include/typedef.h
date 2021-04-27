#ifndef TYPEDEF_H
#define TYPEDEF_H

typedef enum TokenKind TokenKind;
typedef enum ExpressionKind ExpressionKind;
typedef enum PrimaryKind PrimaryKind;
typedef enum UnaryKind UnaryKind;
typedef enum BinaryKind BinaryKind;
typedef enum StatementKind StatementKind;
typedef enum TypeKind TypeKind;
typedef enum DeclarationKind DeclarationKind;
typedef enum KeywordKind KeywordKind;

typedef struct List List;
typedef struct List ListNode;
typedef struct Parser Parser;
typedef struct String String;
typedef struct Token Token;
typedef struct Lexer Lexer;
typedef struct Expression Expression;
typedef struct Binary Binary;
typedef struct Unary Unary;
typedef struct Primary Primary;
typedef struct Statement Statement;
typedef struct Compound Compound;
typedef struct Comment Comment;
typedef struct Program Program;
typedef struct CodeUnit CodeUnit;
typedef struct Scope Scope;
typedef struct Declaration Declaration;
typedef struct Type Type;
typedef struct PointerType PointerType;
typedef struct BasicType BasicType;
typedef struct Function Function;
typedef struct Variable Variable;
typedef struct UnknownType UnknownType;
typedef struct ReturnStatement ReturnStatement;
typedef struct Typer Typer;
typedef struct Call Call;
typedef struct Loop Loop;
typedef struct Conditional Conditional;
typedef struct Array Array;
typedef struct StructMember StructMember;
typedef struct StructType StructType;
typedef struct StructScope StructScope;
typedef struct Dot Dot;

#endif