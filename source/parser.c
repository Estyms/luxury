// Copyright (C) strawberryhacker.
// 
// This file contains the language parser which transforms the token stream from the lexer into a
// graph representation which resebles the original program.
//
// The most important tree nodes are stataments and expression. Expressions evaluates to some kind 
// of value, whereas statements do not. Statements also covers bigger synactical constructs such as 
// loops and if statements.
// 
// Expressions / statements, and declarations are completely separated. A declaration is something
// that maps a name to a type. Examples are variable and function declaration and typedefs. 
// Declarations does not have any thing to do with the actual code, beside being information for 
// the compiler. Therefore it is not a part of the syntax tree. Instead it is placed on the scope.
//
// A scope is a structure which keeps track of all the declarations within a code-block (curly 
// braces). Each scope has a pointer to the parent, used when we are looking up a declaration that 
// is not in the current scope. It also contains a list of all the sub-scopes, used for iterating
// over all declarations in a function, needed for the stack frame allocation.

#include <parser.h>
#include <stdlib.h>
#include <list.h>
#include <assert.h>
#include <error.h>
#include <typer.h>

static Expression* parse_expression(Parser* parser, s8 priority);
static Expression* parse_unary_expression(Parser* parser);
static Expression* parse_primary_expression(Parser* parser);
static Expression* parse_suffix_expression(Parser* parser, Expression* previous);
static Statement* parse_compound_statement(Parser* parser);
static Statement* parse_expression_statement(Parser* parser);
static Statement* parse_block(Parser* parser);
static Statement* parse_compound_statement(Parser* parser);
static Type* parse_struct_declaration(Parser* parser, bool is_anonymous);

static void push_declaration_on_scope(Declaration* declaration, Scope* scope);
static void push_declaration_on_current_scope(Declaration* declaration, Parser* parser);
static Type* parse_type(Parser* parser);
static void parse_function_argument(Parser* parser);
static bool try_parse_declaration(Parser* parser, Statement** statement);
static Scope* enter_scope(Parser* parser);
static void exit_scope(Parser* parser);

// All initial calls to parse_expression must use this initial priority.
static const s8 EXPRESSION_INIT_PRIORITY = -1;

// Since the lexer is not allocating any memory for the tokens (except for the initial fixed size 
// token buffer), we need to manually copy all tokens we want to store.
static Token* copy_token(Token* token) {
    Token* new_token = calloc(1, sizeof(Token));

    u8* source = (u8 *)token;
    u8* dest   = (u8 *)new_token;

    for (u32 i = 0; i < sizeof(Token); i++) {
        *dest++ = *source++;
    }

    return new_token;
}

static BinaryKind token_to_binary_kind(Token* token) {
    switch (token->kind) {
        case TOKEN_EQUAL          : return BINARY_EQUAL;
        case TOKEN_NOT_EQUAL      : return BINARY_NOT_EQUAL;
        case TOKEN_GREATER        : return BINARY_GREATER;
        case TOKEN_GREATER_EQUAL  : return BINARY_GREATER_EQUAL;
        case TOKEN_LESS           : return BINARY_LESS;
        case TOKEN_LESS_EQUAL     : return BINARY_LESS_EQUAL;
        case TOKEN_MINUS          : return BINARY_MINUS;
        case TOKEN_PLUS           : return BINARY_PLUS;
        case TOKEN_DIVISION       : return BINARY_DIVISION;
        case TOKEN_MULTIPLICATION : return BINARY_MULTIPLICATION;
        case TOKEN_ASSIGN         : return BINARY_ASSIGN;
    }

    return 0;
};

static s8 get_binary_precedence(Token* token) {
    switch (token->kind) {
        case TOKEN_MULTIPLICATION:
        case TOKEN_DIVISION:
            return 30;
        case TOKEN_PLUS:
        case TOKEN_MINUS: 
            return 24;
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
            return 20;
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
            return 19;
        case TOKEN_ASSIGN:
            return 1;
    }

    // No valid binary operator.
    return 0;
}

// For parsing all expressions we are using a recursive parser which tracks the running priority of
// the binary operator. Each call will parse a unary expression (left hand side) and then check the 
// next binary priority. If the priority rises we recursivly call parse_expression (which will 
// become the right hand side).
// 
// When the recursive call returns, we have the original left hand side expression (parse_unary) and
// a new right hand side expression (parse_expression). We make a new binary node from these 
// expressions, and treat this as the left hand side expression, and check the next priority.
//
// This way the operator precedence determines in which direction we build the tree. If the tree 
// grows down the left leg (operator rises), or if it grows down the right leg (operator falls).
static Expression* parse_expression(Parser* parser, s8 priority) {
    assert(parser);
    Lexer* lexer = parser->lexer;
    Expression* left = parse_unary_expression(parser);

    while (1) {
        Token* token = current_token(lexer);

        s8 new_priority = get_binary_precedence(token);
        
        // The zero check termiates the recursion if the expression ends.
        if (new_priority == 0 || new_priority <= priority) {
            return left;
        }

        Binary* binary = new_binary(token_to_binary_kind(token));

        binary->operator = copy_token(consume_token(lexer));
        binary->left     = left;
        binary->right    = parse_expression(parser, new_priority);

        // Treat the binary expression as the left hand side.
        left = (Expression *)binary;
    }
}

static Expression* parse_unary_expression(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    if (token->kind == TOKEN_OPEN_PARENTHESIS) {
        // Parenthesised expression.
        Expression* expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
        skip_token(lexer, TOKEN_CLOSE_PARENTHESIS);

        // We might still have a suffix expression following a parenthesized expression e.g.
        // (data + 2)[4] should work assuming data is a pointer.
        return parse_suffix_expression(parser, expression);
    }
    else if (token->kind == TOKEN_MULTIPLICATION) {
        // Address of.
        Unary* unary = new_unary(UNARY_ADDRESS_OF);

        unary->operator = copy_token(token);
        unary->operand  = parse_unary_expression(parser);

        return (Expression *)unary;
    }
    else if (token->kind == TOKEN_AT) {
        // Dereference.
        Unary* unary = new_unary(UNARY_DEREF);

        unary->operator = copy_token(token);
        unary->operand  = parse_unary_expression(parser);

        return (Expression *)unary;
    }

    undo_next_token(lexer);
    
    Expression* primary = parse_primary_expression(parser);
    return parse_suffix_expression(parser, primary);
}

static Expression* parse_primary_expression(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    Primary* primary = new_expression(EXPRESSION_PRIMARY);

    primary->token = copy_token(token);

    switch (token->kind) {
        case TOKEN_NUMBER : {
            primary->kind   = PRIMARY_NUMBER;
            primary->number = token->number;
            break;
        }
        case TOKEN_IDENTIFIER : {
            primary->kind = PRIMARY_IDENTIFIER;
            primary->name = primary->token->name;
            break;
        }
        case TOKEN_STRING : {
            primary->kind   = PRIMARY_STRING;
            primary->string = token->name;
            break;
        }
        default : {
            error_token(token, "not a primary expression");
        }
    }

    return (Expression *)primary;
}

static Expression* parse_suffix_expression(Parser* parser, Expression* previous) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    if (token->kind == TOKEN_OPEN_PARENTHESIS) {
        // Function call expression.
        Call* call = new_call();

        call->expression = previous;
        call->token      = copy_token(token);

        token = current_token(lexer);

        while (token->kind != TOKEN_CLOSE_PARENTHESIS && token->kind != TOKEN_END_OF_FILE) {

            Expression* expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
            list_add_last(&expression->list_node, &call->arguments);
            
            token = current_token(lexer);

            if (token->kind != TOKEN_CLOSE_PARENTHESIS) {
                token = skip_token(lexer, TOKEN_COMMA);
            }
        }

        skip_token(lexer, TOKEN_CLOSE_PARENTHESIS);
        return parse_suffix_expression(parser, (Expression *)call);
    }
    else if (token->kind == TOKEN_OPEN_SQUARE) {
        // Array expression.
        // We do not have any separate structure for the array expresion since it is basically just 
        // a deref. Thus we convert array[10] to *(array + 10).
        Unary* unary = new_expression(EXPRESSION_UNARY);
        Binary* binary = new_binary(BINARY_PLUS);

        binary->operator = copy_token(token);
        binary->left     = previous;
        binary->right    = parse_expression(parser, EXPRESSION_INIT_PRIORITY);

        unary->kind     = UNARY_DEREF;
        unary->operator = binary->operator;
        unary->operand  = (Expression *)binary;

        skip_token(lexer, TOKEN_CLOSE_SQUARE);
        return parse_suffix_expression(parser, (Expression *)unary);
    }
    else if (token->kind == TOKEN_DOT) {
        // Struct member access.
        Dot* dot = new_expression(EXPRESSION_DOT);

        dot->dot_token  = copy_token(token);
        dot->member     = copy_token(current_token(lexer));
        dot->expression = previous;
        
        skip_token(lexer, TOKEN_IDENTIFIER);
        return parse_suffix_expression(parser, (Expression *)dot);
    }

    undo_next_token(lexer);
    return previous;
}

static Statement* parse_expression_statement(Parser* parser) {
    Statement* statement = new_statement(STATEMENT_EXPRESSION);
    
    statement->expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);

    skip_token(parser->lexer, TOKEN_SEMICOLON);
    return statement;
}

static Statement* parse_conditional_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = next_token(lexer);

    Conditional* conditional = new_statement(STATEMENT_CONDITIONAL);

    conditional->condition = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
    conditional->true_body = parse_compound_statement(parser);

    token = current_token(lexer);

    if (is_keyword(token, KEYWORD_ELSE)) {
        token = next_token(lexer);

        if (is_keyword(token, KEYWORD_IF)) {
            conditional->false_body = parse_conditional_statement(parser);
        }
        else {
            conditional->false_body = parse_compound_statement(parser);
        }
    }

    return (Statement *)conditional;
}

static Statement* parse_while_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = next_token(lexer);

    Loop* loop = new_statement(STATEMENT_LOOP);

    loop->condition = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
    loop->body      = parse_compound_statement(parser);

    return (Statement *)loop;
}

// // Fix this crap.
static Statement* parse_for_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = expect_token(lexer, TOKEN_IDENTIFIER);

    Declaration* declaration = new_declaration();

    declaration->kind       = DECLARATION_VARIABLE;
    declaration->name_token = copy_token(token);
    declaration->name       = token->name;
    declaration->type       = new_type(TYPE_INFERRED);

    Loop* loop = new_statement(STATEMENT_LOOP);

    next_token(lexer);
    skip_keyword(lexer, KEYWORD_IN);

    Primary* name = new_primary(PRIMARY_IDENTIFIER);
    name->name = declaration->name;
    name->declaration = declaration;
    
    Binary* assign = new_binary(BINARY_ASSIGN);
    //assign->operator = declaration->name_token;
    assign->left     = (Expression *)name;
    assign->right    = parse_expression(parser, EXPRESSION_INIT_PRIORITY);

    Statement* expr_statement = new_statement(STATEMENT_EXPRESSION);
    expr_statement->expression = (Expression *)assign;

    skip_token(lexer, TOKEN_DOUBLE_DOT);

    Binary* less_equal = new_binary(BINARY_LESS_EQUAL);
    //less_equal->operator = declaration->name_token;
    less_equal->left     = (Expression *)name;
    less_equal->right    = parse_expression(parser, EXPRESSION_INIT_PRIORITY);

    Primary* one = new_primary(PRIMARY_NUMBER);
    //one->token  = declaration->name_token;
    one->number = 1;

    Binary* post = new_binary(BINARY_PLUS);
    //post->operator = declaration->name_token;
    post->left     = (Expression *)name;
    post->right    = (Expression *)one;
    
    assign = new_binary(BINARY_ASSIGN);
    //assign->operator = declaration->name_token;
    assign->left     = (Expression *)name;
    assign->right    = (Expression *)post;

    Statement* post_statement = new_statement(STATEMENT_EXPRESSION);
    post_statement->expression = (Expression *)assign;

    loop->init_statement = expr_statement;
    loop->condition      = (Expression *)less_equal;
    loop->post_statement = post_statement;
    loop->body           = parse_compound_statement(parser);

    push_declaration_on_scope(declaration, loop->body->compound.scope);

    return (Statement *)loop;
}

static Statement* parse_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

    if (token->kind == TOKEN_COMMENT) {
        Statement* statement = new_statement(STATEMENT_COMMENT);
        statement->comment.token = copy_token(token);
        skip_token(lexer, TOKEN_COMMENT);
        return statement;
    }
    else if (token->kind == TOKEN_OPEN_CURLY) {
        return parse_compound_statement(parser);
    }
    else if (is_keyword(token, KEYWORD_RETURN)) {
        ReturnStatement* Return = new_statement(STATEMENT_RETURN);
        skip_token(lexer, TOKEN_IDENTIFIER);
        Return->return_expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
        skip_token(lexer, TOKEN_SEMICOLON); 
        return (Statement *)Return;
    }
    else if (is_keyword(token, KEYWORD_FOR)) {
        return parse_for_statement(parser);
    }
    else if (is_keyword(token, KEYWORD_IF)) {
        return parse_conditional_statement(parser);
    }
    else if (is_keyword(token, KEYWORD_WHILE)) {
        return parse_while_statement(parser);
    }

    return parse_expression_statement(parser);
}

// Parse compound statement will call this function. This will either parse a declaration or a 
// statement. If we only have a declaration without an init expression, the declaration are just 
// pushed onto the current scope, and we return 0.
static Statement* try_parse_declaration_or_statement(Parser* parser) {
    Statement* statement;
    if (try_parse_declaration(parser, &statement)) {
        if (statement) {
            return statement;
        }

        return 0;
    }

    // Will always return.
    return parse_statement(parser);
}

static Type* parse_type(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    if (is_keyword(token, KEYWORD_U64)) {
        return type_u64;
    }
    else if (is_keyword(token, KEYWORD_U32)) {
        return type_u32;
    }
    else if (is_keyword(token, KEYWORD_U16)) {
        return type_u16;
    }
    else if (is_keyword(token, KEYWORD_U8)) {
        return type_u8;
    }
    else if (is_keyword(token, KEYWORD_S64)) {
        return type_s64;
    }
    else if (is_keyword(token, KEYWORD_S32)) {
        return type_s32;
    }
    else if (is_keyword(token, KEYWORD_S16)) {
        return type_s16;
    }
    else if (is_keyword(token, KEYWORD_S8)) {
        return type_s8;
    }
    else if (is_keyword(token, KEYWORD_CHAR)) {
        return type_char;
    }
    else if (token->kind == TOKEN_MULTIPLICATION) {
        // Pointer.
        PointerType* pointer = new_pointer();
        pointer->pointer_to = parse_type(parser);
        return (Type *)pointer;
    }
    else if (token->kind == TOKEN_OPEN_SQUARE) {
        // Array.
        token = current_token(lexer);
        
        // The array expression must be known at compile-time.
        if (token->kind != TOKEN_NUMBER) {
            error_token(token, "cannot evaluate non-constant expressions currently");
        }

        Type* type = new_pointer();
        type->pointer.count = token->number;

        skip_token(lexer, TOKEN_NUMBER);
        skip_token(lexer, TOKEN_CLOSE_SQUARE);

        type->pointer.pointer_to = parse_type(parser);
        return type;
    }
    else if (token->kind == TOKEN_IDENTIFIER) {
        // At this point we do not know if the identifier is a valid typedef. We mark it as unknown 
        // and resolves it in a later pass.
        Type* type = new_type(TYPE_UNKNOWN);
        type->unknown.token = copy_token(token);
        return type;
    }

    error_token(token, "expecting a type");
}

// The declaration parser is not only restricted to variable declarations, and may parse other 
// declarations as well. This is the reason we have to use a separate function when parsing a 
// function argument.
static void parse_function_argument(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Declaration* declaration = new_declaration();

    declaration->kind       = DECLARATION_VARIABLE;
    declaration->name_token = copy_token(consume_token(lexer));
    declaration->name       = declaration->name_token->name;

    skip_token(lexer, TOKEN_COLON);

    declaration->type = parse_type(parser);

    if (declaration->name_token->kind != TOKEN_IDENTIFIER) {
        error_token(declaration->name_token, "expecting an identifier as a function argument.");
    }

    push_declaration_on_current_scope(declaration, parser);
}

// A struct scope will contain all members within a struct namespace. A struct namespace contains
// all structures and members that can be accessed from the same dot member.
// 
// The scope will be used to accelerate struct member lookup. Since we are not looking up any 
// members in anonymous structures (because an anonymous structure cannot be reached from a dot
// member), only tagged structures will have a struct scope.
static StructScope* enter_struct_scope(Parser* parser) {
    StructScope* scope = new_struct_scope();

    scope->parent = parser->current_struct_scope;
    parser->current_struct_scope = scope;

    return scope;
}

static void exit_struct_scope(Parser* parser) {
    assert(parser->current_struct_scope);
    parser->current_struct_scope = parser->current_struct_scope->parent;
}

static void push_struct_member_on_current_scope(StructMember* member, Parser* parser) {
    // We are not pushing anonymous members on the current scope. They will be tracked by the tree
    // structure instead, the reason being that anonymous struct are never the target for any dot 
    // access.
    if (member->is_anonymous) {
        return;
    }

    list_add_last(&member->scope_node, &parser->current_struct_scope->members);
}

static bool does_struct_member_exist(StructMember* member, Parser* parser) {
    ListNode* it;
    list_iterate(it, &parser->current_struct_scope->members) {
        StructMember* scope_member = list_to_struct(it, StructMember, scope_node);

        assert(scope_member->is_anonymous == false);
        if (string_compare(&member->name, &scope_member->name)) {
            return true;
        }
    }

    return false;
}

// Question: what is happening if using an anonymous top level structure
// token : struct {
//    
// }
// will the structure in this case have a scope or not?
static StructMember* parse_struct_member(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

    assert(parser->current_struct_scope);

    if (token->kind != TOKEN_IDENTIFIER) {
        error_token(token, "expecting either a tag or a struct / union keyword.");
    }

    StructMember* member = new_struct_member();
    member->is_anonymous = true;

    if (!is_keyword(token, KEYWORD_STRUCT) && !is_keyword(token, KEYWORD_UNION)) {
        // Tagged struct or a regular struct member.
        member->token        = copy_token(token);
        member->name         = token->name;
        member->is_anonymous = false;

        token = skip_token(lexer, TOKEN_IDENTIFIER);
        token = skip_token(lexer, TOKEN_COLON);
    }

    if (is_keyword(token, KEYWORD_STRUCT) || is_keyword(token, KEYWORD_UNION)) {
        member->type = parse_struct_declaration(parser, member->is_anonymous);
    }
    else {
        member->type = parse_type(parser);
        skip_token(lexer, TOKEN_SEMICOLON);
    }

    return member;
}

static Type* parse_struct_declaration(Parser* parser, bool is_anonymous) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    StructType* type = new_struct();
    type->is_struct = is_keyword(token, KEYWORD_STRUCT);
    
    // If this is a tagged structure, create a new scope.
    if (!is_anonymous) {
        type->scope = enter_struct_scope(parser);
    }

    token = skip_token(lexer, TOKEN_OPEN_CURLY);

    while (token->kind != TOKEN_CLOSE_CURLY && token->kind != TOKEN_END_OF_FILE) {
        StructMember* member = parse_struct_member(parser);
        
        list_add_last(&member->list_node, &type->members);

        
        if (does_struct_member_exist(member, parser)) {
            error_token(member->token, "struct declaration is defined before");
        }

        push_struct_member_on_current_scope(member, parser);
        token = current_token(lexer);
    }

    skip_token(lexer, TOKEN_CLOSE_CURLY);

    if (!is_anonymous) {
        exit_struct_scope(parser);
    }

    return (Type *)type;
}

// If a declaration is parsed successfully and pushed to the scope, this funciton returns true. If
// the declaration also contains an init expression, we convert it into an expression statement and 
// return that in the init_statement.
static bool try_parse_declaration(Parser* parser, Statement** init_statement) {
    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);
    Token* next  = peek_next(lexer);

    *init_statement = 0;

    if (token->kind != TOKEN_IDENTIFIER) {
        return false;
    }

    if (next->kind != TOKEN_COLON && next->kind != TOKEN_DOUBLE_COLON) {
        return false;
    }

    Declaration* declaration = new_declaration();

    declaration->name_token = copy_token(token);
    declaration->name       = token->name;

    bool is_typedef = (next->kind == TOKEN_DOUBLE_COLON);

    token = next_token(lexer);   // Skip the declaration name.
    token = next_token(lexer);   // Skip the :: or :

    if ((is_keyword(token, KEYWORD_FUNC) || is_keyword(token, KEYWORD_ASM)) && !is_typedef) {
        declaration->kind = DECLARATION_FUNCTION;
    
        // Each function contains at least two scopes. The first scope is opened here, and will 
        // only contain the function argument declarations. The second scope is opened automatically
        // by the compound statement.
        Scope* scope = enter_scope(parser);
        Function* function = &declaration->function;

        function->function_scope    = scope;
        function->assembly_function = is_keyword(token, KEYWORD_ASM);

        token = skip_token(lexer, TOKEN_IDENTIFIER);
        token = skip_token(lexer, TOKEN_OPEN_PARENTHESIS);
        
        // Parse the function argumenets.
        while (token->kind != TOKEN_CLOSE_PARENTHESIS && token->kind != TOKEN_END_OF_FILE) {
            parse_function_argument(parser);

            token = current_token(lexer);

            if (token->kind != TOKEN_CLOSE_PARENTHESIS) {
                token = skip_token(lexer, TOKEN_COMMA);
            }
        }

        token = skip_token(lexer, TOKEN_CLOSE_PARENTHESIS);

        // Parse the function return type.
        if (token->kind == TOKEN_ARROW) {
            skip_token(lexer, TOKEN_ARROW);
            function->return_type = parse_type(parser);
        }

        // Fix: this has to be fixed.
        if (function->assembly_function) {
            token = skip_token(lexer, TOKEN_OPEN_CURLY);

            function->assembly_body = current_token(lexer)->name;

            while (token->kind != TOKEN_CLOSE_CURLY && token->kind != TOKEN_END_OF_FILE) {
                token = next_token(lexer);
            }

            function->assembly_body.size = token->name.text - function->assembly_body.text;
            skip_token(lexer, TOKEN_CLOSE_CURLY);
        }
        else {
            function->body = parse_compound_statement(parser);
        }

        exit_scope(parser);
        push_declaration_on_current_scope(declaration, parser);
        return true;
    }
    else if (is_keyword(token, KEYWORD_STRUCT) || is_keyword(token, KEYWORD_UNION)) {
        declaration->kind = (is_typedef) ? DECLARATION_TYPE : DECLARATION_VARIABLE;
        declaration->type = parse_struct_declaration(parser, false);

        assert(parser->current_struct_scope == 0);

        push_declaration_on_current_scope(declaration, parser);
        return true;
    }
    else if (token->kind == TOKEN_ASSIGN && !is_typedef) {
        // Inferred type.
        declaration->kind = DECLARATION_VARIABLE;
        declaration->type = new_type(TYPE_INFERRED);
    }
    else {
        // Either variable or type declaration.
        // var :  u32;
        // var :: u32;
        declaration->kind = (is_typedef) ? DECLARATION_TYPE : DECLARATION_VARIABLE;
        declaration->type = parse_type(parser);
    }

    assert(declaration->type);
    assert(declaration->kind);
    
    push_declaration_on_current_scope(declaration, parser);

    // If the declaration contains an init expression we are parsing that here.
    // Todo: how should we handle global scope?
    token = current_token(lexer);
    if (token->kind == TOKEN_ASSIGN) {

        Binary* assign = new_binary(BINARY_ASSIGN);
        Primary* primary = new_primary(PRIMARY_IDENTIFIER);
        Statement* statement = new_statement(STATEMENT_EXPRESSION);

        primary->token = declaration->name_token;
        primary->name  = declaration->name;
        
        assign->operator = copy_token(consume_token(lexer)); // Skip the assign token.
        assign->left     = (Expression *)primary;
        assign->right    = parse_expression(parser, EXPRESSION_INIT_PRIORITY);

        statement->expression = (Expression *)assign;


        // Return the assign expression from the function.
        *init_statement = statement;
    }

    skip_token(lexer, TOKEN_SEMICOLON);
    return true;
}

static Scope* enter_scope(Parser* parser) {
    Scope* previous_scope = parser->current_scope;
    Scope* scope = new_scope();

    if (previous_scope) {
        list_add_last(&scope->list_node, &previous_scope->child_scopes);
    }

    scope->parent = previous_scope;
    parser->current_scope = scope;

    return scope;
}

static void exit_scope(Parser* parser) {
    assert(parser->current_scope);
    parser->current_scope = parser->current_scope->parent;
}

static Declaration* does_declaration_exist(Declaration* declaration, Scope* scope) {
    assert(declaration && scope);

    List* list = 0;
    if (declaration->kind == DECLARATION_VARIABLE) {
        list = &scope->variables;
    }
    else if (declaration->kind == DECLARATION_FUNCTION) {
        list = &scope->functions;
    }
    else if (declaration->kind == DECLARATION_TYPE) {
        list = &scope->types;
    }

    ListNode* it;
    list_iterate(it, list) {
        Declaration* new_decl = list_to_struct(it, Declaration, list_node);

        if (string_compare(&declaration->name, &new_decl->name)) {
            return new_decl;
        }
    }

    return 0;
}

static void push_declaration_on_scope(Declaration* declaration, Scope* scope) {
    assert(declaration && scope);

    List* list = 0;
    if (declaration->kind == DECLARATION_VARIABLE) {
        list = &scope->variables;
    }
    else if (declaration->kind == DECLARATION_FUNCTION) {
        list = &scope->functions;
    }
    else if (declaration->kind == DECLARATION_TYPE) {
        list = &scope->types;
    }

    if (list == 0) {
        printf("Parser (push_declraration) : declaration type not handled\n");
        exit(1);
    }

    if (does_declaration_exist(declaration, scope)) {
        error_token(declaration->name_token, "declraration is existing");
    }

    list_add_last(&declaration->list_node, list);
}

static void push_declaration_on_current_scope(Declaration* declaration, Parser* parser) {
    push_declaration_on_scope(declaration, parser->current_scope);
}

static Statement* parse_block(Parser* parser) {
    Scope* scope = enter_scope(parser);
    Compound* compound = new_compound_statement();
    compound->scope = scope;

    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

    while (token->kind != TOKEN_END_OF_FILE && token->kind != TOKEN_CLOSE_CURLY) {
        Statement* statement = try_parse_declaration_or_statement(parser);

        if (statement) {
            list_add_last(&statement->list_node, &compound->statements);
        }

        token = current_token(lexer);
    }

    exit_scope(parser);
    return (Statement *)compound;
}

static Statement* parse_compound_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;

    skip_token(lexer, TOKEN_OPEN_CURLY);
    Statement* statement = parse_block(parser);
    skip_token(lexer, TOKEN_CLOSE_CURLY);

    return statement;
}

Parser* new_parser(Lexer* lexer) {
    Parser* parser = calloc(1, sizeof(Parser));
    parser->lexer = lexer;

    // This must be called prior to using the lexer.
    next_token(lexer);
    return parser;
}

static CodeUnit* parser_code_unit(Parser* parser) {
    Statement* statement = parse_block(parser);
    assert(statement->kind == STATEMENT_COMPOUND);
    assert(statement->compound.scope->parent == 0);

    // Go over all the global variables and mask everything as global.
    ListNode* it;
    list_iterate(it, &statement->compound.scope->variables) {
        Declaration* declaration = list_to_struct(it, Declaration, list_node);

        declaration->is_global = true;
    }

    CodeUnit* code_unit = new_code_unit();

    code_unit->global_scope = statement->compound.scope;
    code_unit->file_name = parser->lexer->file_name;

    return code_unit;
}

Program* parser_program(Parser* parser) {
    Program* program = new_program();

    // Todo: this only parses one file; more specifically the file that is in the parser->lexer. So
    // this will require more logic.
    CodeUnit* code_unit = parser_code_unit(parser);
    list_add_last(&code_unit->list_node, &program->code_units);

    return program;
}
