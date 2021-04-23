#ifndef TYPEDEF_H
#define TYPEDEF_H

typedef enum TokenKind TokenKind;
typedef enum ExpressionKind ExpressionKind;
typedef enum PrimaryKind PrimaryKind;
typedef enum UnaryKind UnaryKind;
typedef enum BinaryKind BinaryKind;
typedef enum StatementKind StatementKind;

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

#endif