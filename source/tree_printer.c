#include <tree_printer.h>
#include <list.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#define MAX_INDENTATION 32

u32 indentation;
bool mask[MAX_INDENTATION];

static void print_scope(Scope* scope);
static void print_type(Type* type);
static void print_expression(Expression* expression);
static void print_function(Declaration* decl);
static void print_statement(Statement* statement);
static void print_code_unit(CodeUnit* code_unit);

void indented_print(const char* data, ...) {
    if (indentation) {
        for (u32 i = 0; i < (indentation - 1); i++) {
            if (mask[i]) {
                printf("|   ");
            }
            else {
                printf("    ");
            }
        }

        printf("|-> ");
    }

    static char buffer[1024];

    va_list arg;
    va_start(arg, data);
    u32 size = vsnprintf(buffer, 1024, data, arg);
    va_end(arg);

    printf("%.*s", size, buffer);
}

static const char* unary_kind[] = {
    "none"
};

static const char* binary_kind[] = {
    "none", "+", "-", "*", "/", "==", "<", "<=", ">", ">=", "="
};

static void print_expression(Expression* expression) {
    switch (expression->kind) {
        case EXPRESSION_UNARY : {
            Unary* unary = &expression->unary;
            
            assert(unary->kind < UNARY_KIND_COUNT);
            indented_print("Unary: %s\n", unary_kind[unary->kind]);
            indentation++;
            print_expression(unary->operand);
            indentation--;
            break;
        }
        case EXPRESSION_BINARY : {
            Binary* binary = &expression->binary;

            assert(binary->kind < BINARY_KIND_COUNT);
            indented_print("Binary: %s\n", binary_kind[binary->kind]);
            u32 binary_indent = indentation++;
            
            mask[binary_indent] = true;            
            print_expression(binary->left);
            mask[binary_indent] = false;
            print_expression(binary->right);

            indentation--;
            break;
        }
        case EXPRESSION_PRIMARY : {
            Primary* primary = &expression->primary;
            
            if (primary->kind == PRIMARY_NUMBER) {
                indented_print("Number : %d\n", primary->number);
            }
            else if (primary->kind == PRIMARY_IDENTIFIER) {
                String name = primary->name;
                indented_print("Identifier : %.*s\n", name.size, name.text);

                if (primary->declaration) {
                    assert(primary->declaration->type);
                    indentation++;
                    print_type(primary->declaration->type);
                    indentation--;
                }
            }
            else {
                printf("Primary not handled\n");
                exit(1);
            }
            break;
        }
    }    
}

static void print_function(Declaration* decl) {
    Function* function = &decl->function;

    String name = decl->name;
    indented_print("Function: %.*s\n", name.size, name.text);

    u32 function_indent = indentation++;
    mask[function_indent] = true;

    indented_print("Arguments: \n");
    indentation++;
    print_scope(function->function_scope);
    indentation--;
    mask[function_indent] = false;
    print_statement(function->body);
    
    indentation--;
}

static void print_type(Type* type) {
    switch (type->kind) {
        case TYPE_UNKNOWN : {
            String name = type->unknown.token->name;
            indented_print("Unknown : %.*s\n", name.size, name.text);
            break;
        }
        case TYPE_INFERRED : {
            indented_print("Inferred\n");
            break;
        }
        case TYPE_POINTER : {
            if (type->pointer.count) {
                indented_print("Array of : [%d]\n", type->pointer.count);
            }
            else {
                indented_print("Pointer to :\n");
            }

            indentation++;
            print_type(type->pointer.pointer_to);
            indentation--;
            break;
        }
        case TYPE_BASIC : {
            indented_print("%s %d byte%c\n", (type->basic.is_signed) ? "Signed" : "Unsigned", type->size, (type->size > 1) ? 's' : ' ');
            break;
        }
    }
}

static bool scope_is_clear(Scope* scope) {
    return list_is_empty(&scope->functions) &&
            list_is_empty(&scope->variables) &&
            list_is_empty(&scope->types);
}

// This will print the scope content.
static void print_scope(Scope* scope) {
    u32 scope_indent = indentation - 1;
    mask[scope_indent] = true;

    ListNode* it;
    list_iterate(it, &scope->functions) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_FUNCTION);

        if (it->next == &scope->functions && list_is_empty(&scope->variables) && list_is_empty(&scope->types)) {
            mask[scope_indent] = false;
        }

        print_function(decl);
    }

    list_iterate(it, &scope->variables) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_VARIABLE);

        if (it->next == &scope->variables && list_is_empty(&scope->types)) {
            mask[scope_indent] = false;
        }

        String name = decl->name;
        indented_print("Declaration : %.*s\n", name.size, name.text);

        indentation++;
        print_type(decl->type);
        indentation--;
    }

    // This is not a type, but a typedef.
    list_iterate(it, &scope->types) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_TYPE);

        if (it->next == &scope->types) {
            mask[scope_indent] = false;
        }

        String name = decl->name;
        indented_print("Typedef: %.*s\n", name.size, name.text);
        indentation++;
        print_type(decl->type);
        indentation--;
    }

    mask[scope_indent] = false;
}

static void print_statement(Statement* statement) {
    switch (statement->kind) {
        case STATEMENT_COMPOUND : {
            Compound* compound = &statement->compound;
            indented_print("Compound:\n");

            u32 compound_ident = indentation++;
            mask[compound_ident] = true;

            ListNode* it;
            list_iterate(it, &compound->statements) {
                Statement* new = list_to_struct(it, Statement, list_node);

                if (it->next == &compound->statements && scope_is_clear(compound->scope)) {
                    mask[compound_ident] = false;
                }

                print_statement(new);
            }

            print_scope(compound->scope);

            indentation--;
            mask[compound_ident] = false;
            break;
        }
        case STATEMENT_EXPRESSION : {
            indented_print("Expression:\n");
            indentation++;
            print_expression(statement->expression);
            indentation--;
            break;
        }
        case STATEMENT_RETURN : {
            indented_print("Return : \n");
            indentation++;
            print_expression(statement->return_statement.return_expression);
            indentation--;
            break;
        }
        case STATEMENT_COMMENT : {
            break;
        }
        default : {
            printf("Statement kind not handled %d\n", statement->kind);
            exit(1);
        }
    }
}

static void print_code_unit(CodeUnit* code_unit) {
    assert(code_unit->file_name.text);
    
    String name = code_unit->file_name;
    indented_print("Code unit: %.*s\n", name.size, name.text);
    indentation++;
    print_scope(code_unit->global_scope);
    indentation--;
}

void print_program(Program* program) {
    indented_print("Program: \n");
    u32 program_indent = indentation++;
    mask[program_indent] = true;

    ListNode* it;
    list_iterate(it, &program->code_units) {
        CodeUnit* code_unit = list_to_struct(it, CodeUnit, list_node);

        if (it->next == &program->code_units) {
            mask[program_indent] = false;
        }
        print_code_unit(code_unit);
    }

    mask[program_indent] = false;
    indentation--;
    assert(indentation == 0);
}