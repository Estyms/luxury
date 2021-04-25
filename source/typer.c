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
static Declaration* lookup_in_current_scope(Typer* typer, String* name, DeclarationKind kind);
static Declaration* lookup_in_scope(Scope* scope, String* name, DeclarationKind kind);
static void exit_scope(Typer* typer);
static void enter_scope(Typer* typer, Scope* scope);

// Built-in types.
Type* type_u64  = &(Type){ .kind = TYPE_BASIC, .alignment = 8, .size = 8, .basic.is_signed = false };
Type* type_u32  = &(Type){ .kind = TYPE_BASIC, .alignment = 4, .size = 4, .basic.is_signed = false };
Type* type_u16  = &(Type){ .kind = TYPE_BASIC, .alignment = 2, .size = 2, .basic.is_signed = false };
Type* type_u8   = &(Type){ .kind = TYPE_BASIC, .alignment = 1, .size = 1, .basic.is_signed = false };
Type* type_s64  = &(Type){ .kind = TYPE_BASIC, .alignment = 8, .size = 8, .basic.is_signed = true };
Type* type_s32  = &(Type){ .kind = TYPE_BASIC, .alignment = 4, .size = 4, .basic.is_signed = true };
Type* type_s16  = &(Type){ .kind = TYPE_BASIC, .alignment = 2, .size = 2, .basic.is_signed = true };
Type* type_s8   = &(Type){ .kind = TYPE_BASIC, .alignment = 1, .size = 1, .basic.is_signed = true };
Type* type_char = &(Type){ .kind = TYPE_BASIC, .alignment = 1, .size = 1, .basic.is_signed = true };

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

static void type_expression(Expression* expression, Typer* typer) {
    switch (expression->kind) {
        case EXPRESSION_BINARY : {
            Binary* binary = &expression->binary;

            assert(binary->left);
            assert(binary->right);

            type_expression(binary->right, typer);
            type_expression(binary->left, typer);

           
            break;
        }
        case EXPRESSION_UNARY : {
            Unary* unary = &expression->unary;

            assert(unary->operand);

            type_expression(unary->operand, typer);
            break;
        }
        case EXPRESSION_PRIMARY : {
            Primary* primary = &expression->primary;

            if (primary->kind == PRIMARY_IDENTIFIER) {
                Declaration* decl = lookup_in_current_scope(typer, &primary->name, DECLARATION_VARIABLE);
                if (decl == 0) {
                    error_token(primary->token, "variables is not declarred");
                }
                
                primary->declaration = decl;
            }
            else if (primary->kind == PRIMARY_NUMBER) {
                expression->type = type_u64;
            }
            break;
        }
        default : {
            printf("Typer : expression not handled\n");
            exit(1);
        }
    }
}

static void type_statement(Statement* statement, Typer* typer) {
    switch (statement->kind) {
        case STATEMENT_COMPOUND : {
            enter_scope(typer, statement->compound.scope);
            ListNode* it;
            list_iterate(it, &statement->compound.statements) {
                Statement* new = list_to_struct(it, Statement, list_node);
                type_statement(new, typer);
            }
            exit_scope(typer);
            break;
        }
        case STATEMENT_RETURN : {
            type_expression(statement->return_statement.return_expression, typer);
            break;
        }
        case STATEMENT_EXPRESSION : {
            type_expression(statement->expression, typer);
            break;
        }
        case STATEMENT_COMMENT : {
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
        Declaration* decl = lookup_in_current_scope(typer, &name, DECLARATION_TYPE);
        if (decl) {
            assert(decl->type);
            return decl->type;
        }
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

    print_program(program);
}
