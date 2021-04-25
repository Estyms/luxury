#include <parser.h>
#include <stdlib.h>
#include <list.h>
#include <assert.h>
#include <error.h>
#include <typer.h>

static Statement* parse_compound_statement(Parser* parser);
static Expression* parse_expression(Parser* parser, s8 priority);
static Expression* parse_unary_expression(Parser* parser);
static Expression* parse_primary_expression(Parser* parser);
static Statement* parse_expression_statement(Parser* parser);

static Type* parse_type(Parser* parser);
static void parse_function_argument(Parser* parser);
static bool try_parse_declaration(Parser* parser, Statement** statement);
static Scope* enter_scope(Parser* parser);
static void exit_scope(Parser* parser);
static void push_declaration_on_scope(Declaration* declaration, Scope* scope);
static void push_declaration_on_current_scope(Declaration* declaration, Parser* parser);

static Statement* parse_block(Parser* parser);
static Statement* parse_compound_statement(Parser* parser);

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
        case TOKEN_ASSIGN         : return BINARY_ASSIGN;
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
        case BINARY_ASSIGN:
            return 1;
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
        
        Binary* binary = new_binary(kind);

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
            primary->kind   = PRIMARY_NUMBER;
            primary->number = token->number;
            break;
        }
        case TOKEN_IDENTIFIER : {
            primary->kind = PRIMARY_IDENTIFIER;
            primary->name = primary->token->name;
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

static Statement* parse_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

    if (token->kind == TOKEN_COMMENT) {
        Statement* statement = new_statement(STATEMENT_COMMENT);
        statement->comment.token = copy_token(token);
        next_token(lexer);
        return statement;
    }

    // Compund statement. 
    if (token->kind == TOKEN_OPEN_CURLY) {
        return parse_compound_statement(parser);
    }

    if (is_keyword(token, KEYWORD_RETURN)) {
        next_token(lexer);  // Skip the return token.
        ReturnStatement* Return = new_statement(STATEMENT_RETURN);

        Return->return_expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
        skip_token(lexer, TOKEN_SEMICOLON);

        return (Statement *)Return;
    }

    return parse_expression_statement(parser);
}

static Statement* try_parse_declaration_or_statement(Parser* parser) {
    Statement* statement;
    if (try_parse_declaration(parser, &statement)) {
        if (statement) {
            return statement;
        }

        return 0;
    }

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
        // Regular pointer.
        PointerType* pointer = new_pointer();
        pointer->pointer_to = parse_type(parser);
        return (Type *)pointer;
    }
    else if (token->kind == TOKEN_OPEN_SQUARE) {
        // Array type.
        token = current_token(lexer);
        
        if (token->kind != TOKEN_NUMBER) {
            error_token(token, "cannot evaluate non-constant expressions currently");
        }

        PointerType* pointer = new_pointer();
        pointer->count = token->number;

        skip_token(lexer, TOKEN_NUMBER);
        skip_token(lexer, TOKEN_CLOSE_SQUARE);

        pointer->pointer_to = parse_type(parser);
        return (Type *)pointer;
    }
    else if (token->kind == TOKEN_IDENTIFIER) {
        Type* type = new_type(TYPE_UNKNOWN);
        type->unknown.token = copy_token(token);
        return type;
    }

    undo_next_token(lexer);
    return 0;
}

static void parse_function_argument(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Declaration* declaration = new_declaration();

    declaration->kind = DECLARATION_VARIABLE;
    declaration->name_token = copy_token(consume_token(lexer));
    skip_token(lexer, TOKEN_COLON);
    declaration->type = parse_type(parser);

    if (declaration->type == 0 || declaration->name_token->kind != TOKEN_IDENTIFIER) {
        error_token(declaration->name_token, "argument error");
    }

    declaration->name = declaration->name_token->name;
    push_declaration_on_current_scope(declaration, parser);
}

// Returns true if a declaration was parsed successfully. It also returns the init expression if
// present, otherwise it just returns 0.
static bool try_parse_declaration(Parser* parser, Statement** init_statement) {
    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);
    Token* next  = peek_next(lexer);

    *init_statement = 0;

    if (token->kind != TOKEN_IDENTIFIER) {
        return false;
    }

    // Check if we have a declaration at all.
    if (next->kind != TOKEN_COLON && next->kind != TOKEN_DOUBLE_COLON) {
        return false;
    }

    Declaration* declaration = new_declaration();

    declaration->name_token = copy_token(token);
    declaration->name       = token->name;

    bool is_typedef = (next->kind == TOKEN_DOUBLE_COLON);

    token = next_token(lexer);   // Skip the identifier.
    token = next_token(lexer);   // Skip the :: or :

    if (is_keyword(token, KEYWORD_FUNC) && !is_typedef) {
        declaration->kind = DECLARATION_FUNCTION;
        // Function.
        Scope* scope = enter_scope(parser);

        Function* function = &declaration->function;
        function->function_scope = scope;

        token = next_token(lexer);  // Skip the function keyword.
        token = skip_token(lexer, TOKEN_OPEN_PARENTHESIS);
        
        // Parse the function argumenets.
        while (token->kind != TOKEN_END_OF_FILE && token->kind != TOKEN_CLOSE_PARENTHESIS) {
            parse_function_argument(parser);

            token = current_token(lexer);
            if (token->kind != TOKEN_COMMA) {
                break;
            }

            token = skip_token(lexer, TOKEN_COMMA);
        }

        token = skip_token(lexer, TOKEN_CLOSE_PARENTHESIS);

        if (token->kind == TOKEN_ARROW) {
            skip_token(lexer, TOKEN_ARROW);
            function->return_type = parse_type(parser);
        }

        function->body = parse_compound_statement(parser);
        exit_scope(parser);

        push_declaration_on_current_scope(declaration, parser);
        return true;
    }
    else if (token->kind == TOKEN_ASSIGN && !is_typedef) {
        // Inferred.
        declaration->kind = DECLARATION_VARIABLE;
        declaration->type = new_type(TYPE_INFERRED);
    }
    else {
        // var :  u32;
        // var :: u32;
        declaration->kind = (is_typedef) ? DECLARATION_TYPE : DECLARATION_VARIABLE;
        declaration->type = parse_type(parser);
        if (declaration->type == 0) {
            error_token(undo_next_token(lexer), "expecting a type or = after the :");
        }
    }

    assert(declaration->type);
    assert(declaration->kind);
    
    push_declaration_on_current_scope(declaration, parser);

    // If the declaration contains an init expression we are parsing that here, and returning
    // that from the function.
    //
    // Todo: global scope.
    token = current_token(lexer);
    if (token->kind == TOKEN_ASSIGN) {

        Binary* assign = new_binary(BINARY_ASSIGN);
        Primary* primary = new_primary(PRIMARY_IDENTIFIER);

        primary->token = declaration->name_token;
        primary->name  = declaration->name;
        
        assign->operator = copy_token(token);
        assign->left     = (Expression *)primary;

        skip_token(lexer, TOKEN_ASSIGN);
        assign->right = parse_expression(parser, EXPRESSION_INIT_PRIORITY);

        Statement* statement = new_statement(STATEMENT_EXPRESSION);
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
        printf("adding a new scope : %p\n", previous_scope);
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
        printf("Declaration type not handled\n");
        exit(1);
    }

    list_add_last(&declaration->list_node, list);
}

static void push_declaration_on_current_scope(Declaration* declaration, Parser* parser) {
    push_declaration_on_scope(declaration, parser->current_scope);
}

static Statement* parse_block(Parser* parser) {
    Scope* scope = enter_scope(parser);

    // Link the compound statement to the current scope.
    Compound* compound = new_compound_statement();
    compound->scope = scope;

    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

    // Parse all the statments in a scope.
    // Todo: we must track if we are in the top level scope.
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

// Returns a compound statement.
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

    next_token(lexer);
    return parser;
}

static CodeUnit* parser_code_unit(Parser* parser) {
    CodeUnit* code_unit = new_code_unit();


    // Where we parse a file.
    Statement* statement = parse_block(parser);
    assert(statement->kind == STATEMENT_COMPOUND);
    code_unit->global_scope = statement->compound.scope;
    code_unit->file_name = parser->lexer->file_name;

    return code_unit;
}

Program* parser_program(Parser* parser) {
    Program* program = new_program();

    CodeUnit* code_unit = parser_code_unit(parser);
    list_add_last(&code_unit->list_node, &program->code_units);

    return program;
}