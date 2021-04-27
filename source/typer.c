#include <typer.h>
#include <stdio.h>
#include <tree_printer.h>
#include <string.h>
#include <assert.h>
#include <error.h>
#include <stdlib.h>
#include <list.h>

static void type_scope(Scope* scope, Typer* typer);
static void type_code_unit(CodeUnit* code_unit, Typer* typer);
static void type_function(Declaration* decl, Typer* typer);
static void type_statement(Statement* statement, Typer* typer);
static void type_expression(Expression* expression, Typer* typer);
static void type_binary_expression(Expression* expression, Typer* typer);
static void type_unary_expression(Expression* expression, Typer* typer);
static void type_primary_expression(Expression* expression, Typer* typer);
static void type_call_expression(Expression* expression, Typer* typer);

// Built-in types.
Type* type_u64  = &(Type){ .kind = TYPE_BASIC, .alignment = 8, .size = 8, .basic.is_signed = false };
Type* type_u32  = &(Type){ .kind = TYPE_BASIC, .alignment = 4, .size = 4, .basic.is_signed = false };
Type* type_u16  = &(Type){ .kind = TYPE_BASIC, .alignment = 2, .size = 2, .basic.is_signed = false };
Type* type_u8   = &(Type){ .kind = TYPE_BASIC, .alignment = 1, .size = 1, .basic.is_signed = false };
Type* type_s64  = &(Type){ .kind = TYPE_BASIC, .alignment = 8, .size = 8, .basic.is_signed = true  };
Type* type_s32  = &(Type){ .kind = TYPE_BASIC, .alignment = 4, .size = 4, .basic.is_signed = true  };
Type* type_s16  = &(Type){ .kind = TYPE_BASIC, .alignment = 2, .size = 2, .basic.is_signed = true  };
Type* type_s8   = &(Type){ .kind = TYPE_BASIC, .alignment = 1, .size = 1, .basic.is_signed = true  };
Type* type_char = &(Type){ .kind = TYPE_BASIC, .alignment = 1, .size = 1, .basic.is_signed = true  };

static void enter_scope(Typer* typer, Scope* scope) {
    typer->current_scope = scope;
}

static void exit_scope(Typer* typer) {
    assert(typer->current_scope);
    typer->current_scope = typer->current_scope->parent;
}

static Declaration* lookup_in_scope(Scope* scope, String* name, DeclarationKind kind) {
    List* list = 0;
    
    if (kind == DECLARATION_VARIABLE) {
        list = &scope->variables;
    }
    else if (kind == DECLARATION_FUNCTION) {
        list = &scope->functions;
    }
    else if (kind == DECLARATION_TYPE) {
        list = &scope->types;
    }

    ListNode* it;
    list_iterate(it, list) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);

        if (string_compare(name, &decl->name)) {
            return decl;
        }
    }

    if (scope->parent) {
        return lookup_in_scope(scope->parent, name, kind);
    }

    return 0;
}

static Declaration* lookup_in_current_scope(Typer* typer, String* name, DeclarationKind kind) {
    return lookup_in_scope(typer->current_scope, name, kind);
}

static void type_binary_expression(Expression* expression, Typer* typer) {
    Binary* binary = &expression->binary;

    assert(binary->left);
    assert(binary->right);

    type_expression(binary->right, typer);
    type_expression(binary->left, typer);

    if (binary->left->kind == EXPRESSION_PRIMARY && binary->left->primary.kind == PRIMARY_IDENTIFIER) {
        assert(binary->left->primary.declaration);
        if (binary->left->primary.declaration->type->kind == TYPE_INFERRED) {
            assert(binary->right->type);
            binary->left->primary.declaration->type = binary->right->type;
            binary->left->type = binary->right->type;
        }
    } 

    if (binary->kind == BINARY_PLUS) {
        assert(binary->left->type);
        assert(binary->right->type);

        TypeKind right_kind = binary->right->type->kind;
        TypeKind left_kind = binary->left->type->kind;

        if (left_kind != TYPE_POINTER && right_kind != TYPE_POINTER) {
            expression->type = binary->left->type;
            return;
        }

        if (left_kind == TYPE_POINTER && right_kind == TYPE_POINTER) {
            error_token(binary->operator, "cannot use this operator on two pointers");
        }

        if (left_kind != TYPE_POINTER && right_kind == TYPE_POINTER) {
            Expression* temp = binary->left;

            binary->left  = binary->right;
            binary->right = temp;
        }

        // Now we have the case where test + 45, where test is a pointer.
        u32 size = binary->left->type->pointer.pointer_to->size;

        assert(size);

        Primary* primary = new_primary(PRIMARY_NUMBER);
        primary->number = size;

        Binary* mult = new_binary(BINARY_MULTIPLICATION);
        mult->operator = binary->operator;
        mult->left = binary->right;
        mult->right = (Expression *)primary;

        binary->right = (Expression *)mult;  

        type_expression((Expression *)primary, typer);
        type_expression((Expression *)mult, typer);

        expression->type = binary->left->type;
    }
    else if (binary->kind == BINARY_MINUS) {

    }
}

static void type_unary_expression(Expression* expression, Typer* typer) {
    assert(expression);
    assert(typer);

    Unary* unary = &expression->unary;
    assert(unary->operand);

    type_expression(unary->operand, typer);
    assert(unary->operand->type);

    if (unary->kind == UNARY_ADDRESS_OF) {
        Type* type = new_type(TYPE_POINTER);
        type->pointer.pointer_to = unary->operand->type;
        expression->type = type;
    }
    else if (unary->kind == UNARY_DEREF) {
        expression->type = unary->operand->type->pointer.pointer_to;
    }
}

static void type_primary_expression(Expression* expression, Typer* typer) {
    Primary* primary = &expression->primary;

    if (primary->kind == PRIMARY_IDENTIFIER) {
        Declaration* decl = lookup_in_current_scope(typer, &primary->name, DECLARATION_VARIABLE);
        if (decl == 0) {
            error_token(primary->token, "variables is not declarred");
        }

        primary->declaration = decl;
        expression->type = decl->type;
    }
    else if (primary->kind == PRIMARY_NUMBER) {
        expression->type = type_u64;
    }
    else if (primary->kind == PRIMARY_STRING) {
        Type* type = new_pointer();
        type->pointer.pointer_to = type_char;
        expression->type = type;
    }
}

static void type_call_expression(Expression* expression, Typer* typer) {
    Call* call = &expression->call;

    ListNode* it;
    list_iterate(it, &call->arguments) {
        Expression* argument = list_to_struct(it, Expression, list_node);
        type_expression(argument, typer);
    }

    assert(call->expression->kind == EXPRESSION_PRIMARY);
    assert(call->expression->primary.kind == PRIMARY_IDENTIFIER);

    String name = call->expression->primary.name;
    Declaration* decl = lookup_in_current_scope(typer, &name, DECLARATION_FUNCTION);
    if (!decl) {
        //error_token(call->expression->primary.token, "function not found");
    } else {
        expression->type = decl->function.return_type;
    }
}

static void type_expression(Expression* expression, Typer* typer) {
    switch (expression->kind) {
        case EXPRESSION_BINARY : {
            type_binary_expression(expression, typer);
            break;
        }
        case EXPRESSION_UNARY : {
            type_unary_expression(expression, typer);
            break;
        }
        case EXPRESSION_PRIMARY : {
            type_primary_expression(expression, typer);
            break;
        }
        case EXPRESSION_CALL : {
            type_call_expression(expression, typer);
            break;
        }
        default : {
            printf("Typer : expression not handled\n");
            exit(1);
        }
    }
}

static void type_compound_statement(Statement* statement, Typer* typer) {
    Compound* compound = &statement->compound;
    enter_scope(typer, compound->scope);
    ListNode* it;
    list_iterate(it, &compound->statements) {
        Statement* new = list_to_struct(it, Statement, list_node);
        type_statement(new, typer);
    }
    exit_scope(typer);
}

static void type_return_statement(Statement* statement, Typer* typer) {
    ReturnStatement* Return = &statement->Return;
    type_expression(Return->return_expression, typer);
}

static void type_expression_statement(Statement* statement, Typer* typer) {
    Expression* expression = statement->expression;
    type_expression(expression, typer);
}

static void type_conditional_statement(Statement* statement, Typer* typer) {
    Conditional* cond = &statement->conditional;
    type_expression(cond->condition, typer);
    type_statement(cond->true_body, typer);
    if (cond->false_body) {
        type_statement(cond->false_body, typer);
    }
}

static void type_loop_statement(Statement* statement, Typer* typer) {
    Loop* loop = &statement->loop;

    enter_scope(typer, loop->body->compound.scope);
    type_statement(loop->init_statement, typer);
    type_expression(loop->condition, typer);
    type_statement(loop->post_statement, typer);
    exit_scope(typer);


    type_statement(loop->body, typer);
}

static void type_statement(Statement* statement, Typer* typer) {
    switch (statement->kind) {
        case STATEMENT_COMPOUND : {
            type_compound_statement(statement, typer);
            break;
        }
        case STATEMENT_RETURN : {
            type_return_statement(statement, typer);
            break;
        }
        case STATEMENT_EXPRESSION : {
            type_expression_statement(statement, typer);
            break;
        }
        case STATEMENT_COMMENT : {
            break;
        }
        case STATEMENT_CONDITIONAL : {
            type_conditional_statement(statement, typer);
            break;
        }
        case STATEMENT_LOOP : {
            type_loop_statement(statement, typer);
            break;
        }
        default : {
            printf("Typer : statement not handled\n");
            exit(1);
        }
    }
}

static void type_function(Declaration* decl, Typer* typer) {
    Function* function = &decl->function;

    type_scope(function->function_scope, typer);

    enter_scope(typer, function->function_scope);
    assert(function->body->compound.scope != function->function_scope);
    type_scope(function->body->compound.scope, typer);
    type_statement(function->body, typer);
    exit_scope(typer);
}

static Type* resolve_type(Type* type, Typer* typer) {
    if (type->kind == TYPE_POINTER) {
        type->pointer.pointer_to = resolve_type(type->pointer.pointer_to, typer);
        return type;
    }
    else if (type->kind == TYPE_UNKNOWN) {
        String name = type->unknown.token->name;
        printf("Typing : %.*s\n", name.size, name.text);

        Declaration* declaration = lookup_in_current_scope(typer, &name, DECLARATION_TYPE);
        if (declaration) {
            assert(declaration->type);
            return declaration->type;
        }

        error_token(type->unknown.token, "unknown type");
    }

    return type;
}

static void type_scope(Scope* scope, Typer* typer) {
    enter_scope(typer, scope);

    ListNode* it;
    list_iterate(it, &scope->variables) {        
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        decl->type = resolve_type(decl->type, typer);
    }

    list_iterate(it, &scope->functions) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_FUNCTION);

        type_function(decl, typer);
    }    

    exit_scope(typer);
}

static void type_code_unit(CodeUnit* code_unit, Typer* typer) {
    type_scope(code_unit->global_scope, typer);
}

void type_program(Program* program, Typer* typer) {
    typer->current_scope = 0;

    ListNode* it;
    list_iterate(it, &program->code_units) {
        CodeUnit* code_unit = list_to_struct(it, CodeUnit, list_node);

        type_code_unit(code_unit, typer);
    }
}
