// This file will contain the language parser which is transforming the token stream from the lexer
// into a graph representation. The tree nodes are listed in the tree.h header file. The expression
// node and the statement node are the core components in the tree and are used in many other nodes 
// as well; for example the return statement contains a pointer to the return expression. 
// Declarations and acctual statments are completely separated. This is because declarations does 
// not have any function in the code. Declarations are placed on the scope, and will contain a 
// mapping between a name and a type, used in later passes in the type checking phase.

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



static void push_declaration_on_scope(Declaration* declaration, Scope* scope);
static void push_declaration_on_current_scope(Declaration* declaration, Parser* parser);
static Type* parse_type(Parser* parser);
static void parse_function_argument(Parser* parser);
static bool try_parse_declaration(Parser* parser, Statement** statement);
static Scope* enter_scope(Parser* parser);
static void exit_scope(Parser* parser);

// All initial calls to parse_expression must use this initial priority.
static const s8 EXPRESSION_INIT_PRIORITY = -1;

// Todo: remove this.
static void print_token(Token* token) {
    printf("%.*s\n", token->name.size, token->name.text);
}

// Since the lexer is not allocating any memory for the tokens except for the initial fixed size 
// token buffer, we need to manually copy any tokens we want to store.
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
            return 19;
        case TOKEN_ASSIGN:
            return 1;
    }

    // No valid binary operator.
    return 0;
}

// We are using a recursive expression parser which tracks the running priority of the binary 
// operator. Each call will parse a unary expression (left expression) and then check the next 
// binary priority. If the priority rises we recursivly call parse_expression (right expression),
// and construct a new node. Once done we treat this new binary node as the left expression, and
// continue to loop. If the priority does not rise, we stop recursing.
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

        // Treat the binary expression as the left expression.
        left = (Expression *)binary;
    }
}

static Expression* parse_unary_expression(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    if (token->kind == TOKEN_OPEN_PARENTHESIS) {
        // This is handeling all parenthesised sub-expressions.
        Expression* expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
        skip_token(lexer, TOKEN_CLOSE_PARENTHESIS);
        return parse_suffix_expression(parser, expression);
    }
    else if (token->kind == TOKEN_MULTIPLICATION) {
        // Address.
        Unary* unary = new_unary(UNARY_ADDRESS_OF);
        unary->operator = copy_token(token);
        unary->operand  = parse_unary_expression(parser);

        return (Expression *)unary;
    }
    else if (token->kind == TOKEN_AT) {
        // Deref. 
        Unary* unary = new_unary(UNARY_DEREF);
        unary->operator = copy_token(token);
        unary->operand  = parse_unary_expression(parser);

        return (Expression *)unary;
    }

    undo_next_token(lexer);
    
    Expression* primary = parse_primary_expression(parser);
    return parse_suffix_expression(parser, primary);
}

static Expression* parse_suffix_expression(Parser* parser, Expression* previous) {
    Lexer* lexer = parser->lexer;
    Token* token = consume_token(lexer);

    if (token->kind == TOKEN_OPEN_PARENTHESIS) {
        // Function call expression.
        Call* call = new_call();
        call->expression = previous;
        call->token = copy_token(token);

        token = current_token(lexer);
        while (token->kind != TOKEN_CLOSE_PARENTHESIS && token->kind != TOKEN_END_OF_FILE) {
            Expression* new = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
            list_add_last(&new->list_node, &call->arguments);
            
            token = current_token(lexer);
            if (token->kind == TOKEN_CLOSE_PARENTHESIS) {
                break;
            }

            skip_token(lexer, TOKEN_COMMA);
        }

        skip_token(lexer, TOKEN_CLOSE_PARENTHESIS);
        return parse_suffix_expression(parser, (Expression *)call);
    }
    else if (token->kind == TOKEN_OPEN_SQUARE) {
        // Array expression. We convert this to an offsetted deref.
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

    undo_next_token(lexer);
    return previous;
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
            primary->string = token->name;
            primary->kind   = PRIMARY_STRING;
            break;
        }
        default : {
            error_token(token, "not a primary expression");
        }
    }

    assert(primary->kind);
    return (Expression *)primary;
}

static Statement* parse_expression_statement(Parser* parser) {
    Statement* statement = new_statement(STATEMENT_EXPRESSION);
    statement->expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
    skip_token(parser->lexer, TOKEN_SEMICOLON);
    return statement;
}

static Statement* parse_if_statement(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Token* token = next_token(lexer);

    Conditional* cond = new_statement(STATEMENT_CONDITIONAL);

    cond->condition = parse_expression(parser, EXPRESSION_INIT_PRIORITY);
    cond->true_body = parse_compound_statement(parser);

    token = current_token(lexer);
    if (is_keyword(token, KEYWORD_ELSE)) {
        token = next_token(lexer);

        if (is_keyword(token, KEYWORD_IF)) {
            cond->false_body = parse_if_statement(parser);
        }
        else {
            cond->false_body = parse_compound_statement(parser);
        }
    }

    return (Statement *)cond;
}

static Statement* parse_while_statement(Parser* parser) {

}

// for i in 0..5
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
        // User defined compund statement.
        return parse_compound_statement(parser);
    }
    else if (is_keyword(token, KEYWORD_RETURN)) {
        next_token(lexer);  // Skip the return token.

        ReturnStatement* Return = new_statement(STATEMENT_RETURN);
        Return->return_expression = parse_expression(parser, EXPRESSION_INIT_PRIORITY);

        skip_token(lexer, TOKEN_SEMICOLON);
        return (Statement *)Return;
    }
    else if (is_keyword(token, KEYWORD_FOR)) {
        return parse_for_statement(parser);
    }
    else if (is_keyword(token, KEYWORD_IF)) {
        return parse_if_statement(parser);
    }

    return parse_expression_statement(parser);
}

// If this function parses a pure declaration without any init expression, it will not return
// anything. Otherwise, we return either the init expression statement or just a normal statement.
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

        Type* type = new_pointer();
        type->pointer.count = token->number;

        skip_token(lexer, TOKEN_NUMBER);
        skip_token(lexer, TOKEN_CLOSE_SQUARE);

        type->pointer.pointer_to = parse_type(parser);


        type->size = type->pointer.count * type->pointer.pointer_to->size;
        return type;
    }
    else if (token->kind == TOKEN_IDENTIFIER) {
        // The identifier may have a valid reference in the code. We mark it as unknown at this 
        // point. The typer will in a later pass will replace this type.
        Type* type = new_type(TYPE_UNKNOWN);
        type->unknown.token = copy_token(token);
        return type;
    }

    undo_next_token(lexer);
    
    // Todo: I do not know if this should yield an error in all cases. If this function end up 
    // returning null, we have to check for that troughout the code.
    error_token(token, "expecting a type");
}

static void parse_function_argument(Parser* parser) {
    Lexer* lexer = parser->lexer;
    Declaration* declaration = new_declaration();

    declaration->kind = DECLARATION_VARIABLE;
    declaration->name_token = copy_token(consume_token(lexer));
    skip_token(lexer, TOKEN_COLON);
    declaration->type = parse_type(parser);

    if (declaration->name_token->kind != TOKEN_IDENTIFIER) {
        error_token(declaration->name_token, "argument error");
    }

    declaration->name = declaration->name_token->name;
    push_declaration_on_current_scope(declaration, parser);
}

// If a declaration is parsed successfully and pushed to the scope, this funciton returns true. If
// the declaration also contains an init expression, we convert it to an expression statement and 
// return that as an init_statement.
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

    // Typedefs are recognized by a double colon.
    bool is_typedef = (next->kind == TOKEN_DOUBLE_COLON);

    token = next_token(lexer);   // Skip the declaration name.
    token = next_token(lexer);   // Skip the :: or :

    if (is_keyword(token, KEYWORD_FUNC) && !is_typedef) {
        declaration->kind = DECLARATION_FUNCTION;
    
        // Each function contains at least two scopes. The first scope is opened here, and will 
        // only contain the function argument declarations. The second scope is opened automatically
        // by the compound statement.
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

        // Parse the function return type.
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

    if (scope->parent) {
        return does_declaration_exist(declaration, scope->parent);
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

    // Link the compound statement to the current scope. So that we can get the scope just from 
    // looking at the compound statement.
    Compound* compound = new_compound_statement();
    compound->scope = scope;

    Lexer* lexer = parser->lexer;
    Token* token = current_token(lexer);

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
    next_token(lexer);  // This must be done in order to start the lexer.
    return parser;
}

static CodeUnit* parser_code_unit(Parser* parser) {
    Statement* statement = parse_block(parser);
    assert(statement->kind == STATEMENT_COMPOUND);

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