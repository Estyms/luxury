#include <tree_printer.h>
#include <list.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#define MAX_INDENTATION 32

u32 indentation;
bool mask[MAX_INDENTATION];

void indented_print(const char* data, ...) {
    if (indentation) {
        for (u32 i = 0; i < (indentation - 1); i++) {
            if (mask[i]) {
                printf("|  ");
            }
            else {
                printf("   ");
            }
        }

        printf("|-->");
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
    "none", "+", "-", "*", "/", "==", "<", "<=", ">", ">="
};

void print_expression(Expression* expression) {
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
            else {
                printf("Primary not handled\n");
                exit(1);
            }
            break;
        }
    }    
}

void print_statement(Statement* statement) {
    switch (statement->kind) {
        case STATEMENT_COMPOUND : {
            Compound* compound = &statement->compound;
            indented_print("Compound:\n");

            u32 compound_ident = indentation++;
            mask[compound_ident] = true;

            ListNode* it;
            list_iterate(it, &compound->statements) {
                Statement* new = list_to_struct(it, Statement, list_node);

                if (it->next == &compound->statements) {
                    mask[compound_ident] = false;
                }

                print_statement(new);
            }

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
        default : {
            printf("Statement kind not handled\n");
            exit(1);
        }
    }
}