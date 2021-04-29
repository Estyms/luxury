#include <tree_printer.h>
#include <list.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#define MAX_INDENTATION 32


// Fix this.
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

u32 indentation;
bool mask[MAX_INDENTATION];

static void print_scope(Scope* scope);
static void print_type(Type* type, bool print_all);
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

void colored_indented_print(const char* color, const char* data, ...) {
    printf(KNRM);
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

    printf("%s%.*s", color, size, buffer);
    printf(KNRM);
}

static const char* unary_kind[] = {
    "none", "deref", "address of"
};

static const char* binary_kind[] = {
    "none", "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">=", "="
};

static void print_expression(Expression* expression) {
    if (expression->type) {
        print_type(expression->type, false);
    }
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
        case EXPRESSION_DOT : {
            Dot* dot = &expression->dot;

            indented_print("Dot: %.*s\n", dot->member->name.size, dot->member->name.text);
            indentation++;
            print_expression(dot->expression);
            indentation--;
            break;
        }
        case EXPRESSION_CALL : {
            Call* call = &expression->call;

            indented_print("Call:\n");
            u32 call_indent = indentation++;
            mask[call_indent] = true;
            indented_print("Expression: \n");
            indentation++;
            print_expression(call->expression);
            indentation--;

            if (list_is_empty(&call->arguments)) {
                indented_print("Arguments: none\n");
            }
            else {
                ListNode* it;
                list_iterate(it, &call->arguments) {
                    Expression* expr = list_to_struct(it, Expression, list_node);

                    if (it->next == &call->arguments) {
                        mask[call_indent] = false;
                    }

                    indented_print("Argument: \n");
                    indentation++;
                    print_expression(expr);
                    indentation--;
                }
            }
            mask[call_indent] = false;
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
                }
            }
            else if (primary->kind == PRIMARY_STRING) {
                indented_print("String : %.*s\n", primary->name.size, primary->name.text);
            }
            else {
                printf("Primary not handled\n");
                exit(1);
            }
            break;
        }
    }    
}

static void print_asm_body(String* string) {
    indented_print(" Assembly:\n");
    indentation++;
    mask[indentation] = true;

    indented_print(" > ");
    for (u32 i = 0; i < string->size-1; i++) {
        char c = string->text[i];

        if (c == '\n') {
            printf("\n");
            indented_print(" > ");
            continue;
        }

        printf("%c", c);
    }
    mask[indentation] = false;
    indentation--;
    printf("\n");
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

    if (function->assembly_function) {
        print_asm_body(&function->assembly_body);
    }
    else {
        print_statement(function->body);
    }
    
    indentation--;
}

static void print_struct(Type* type, bool print_all) {
    StructType* Struct = &type->Struct;

    colored_indented_print(KGRN, "Struct size: %d align: %d: %s\n", type->size, type->alignment, (Struct->scope) ? "" : "anonymous");
    if (!print_all) {
        return;
    }

    u32 struct_indent = indentation++;
    mask[struct_indent] = true;

    ListNode* it;
    list_iterate(it, &Struct->members) {
        StructMember* member = list_to_struct(it, StructMember, list_node);

        if (it->next == &Struct->members) {
            mask[struct_indent] = false;
        }

        colored_indented_print(KGRN, "Struct member: offset %d\n", member->offset);
        u32 member_indent = indentation++;

        if (member->is_anonymous == false) {
            String name = member->name;
            colored_indented_print(KGRN, "Name : %.*s\n", name.size, name.text);
        }

        assert(member->type);
        print_type(member->type , print_all);
        indentation--;
    }

    mask[struct_indent] = false;
    indentation--;
}

static void print_type(Type* type, bool print_all) {
    switch (type->kind) {
        case TYPE_UNKNOWN : {
            String name = type->unknown.token->name;
            colored_indented_print(KGRN,"Unknown : %.*s\n", name.size, name.text);
            break;
        }
        case TYPE_INFERRED : {
            colored_indented_print(KGRN,"Inferred\n");
            break;
        }
        case TYPE_POINTER : {
            if (type->pointer.count) {
                colored_indented_print(KGRN,"Array of : [%d]\n", type->pointer.count);
            }
            else {
                colored_indented_print(KGRN,"Pointer to :\n");
            }

            indentation++;
            
            printf(KGRN);
            print_type(type->pointer.pointer_to, print_all);
            printf(KNRM);
            indentation--;
            break;
        }
        case TYPE_STRUCT : {
            print_struct(type, print_all);
            break;
        }
        case TYPE_BASIC : {
            colored_indented_print(KGRN,"%s %d byte%c\n", (type->basic.is_signed) ? "Signed" : "Unsigned", type->size, (type->size > 1) ? 's' : ' ');
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
        print_type(decl->type, true);
        indentation--;
    }

    list_iterate(it, &scope->types) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_TYPE);

        if (it->next == &scope->types) {
            mask[scope_indent] = false;
        }

        String name = decl->name;
        indented_print("Typedef: %.*s\n", name.size, name.text);
        indentation++;
        print_type(decl->type, true);
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
        case STATEMENT_LOOP : {
            Loop* loop = &statement->loop;
            u32 loop_indent = indentation;
            indented_print("Loop:\n");
            indentation++;

            mask[loop_indent] = true;

            if (loop->init_statement) {
                indented_print("Init: \n");
                indentation++;
                print_statement(loop->init_statement);
                indentation--;
            }

            indented_print("Condition: \n");
            indentation++;
            print_expression(loop->condition);
            indentation--;

            if (loop->post_statement) {
                indented_print("Post statement: \n");
                indentation++;
                print_statement(loop->post_statement);
                indentation--;
            }

            mask[loop_indent] = false;

            indented_print("Body: \n");
            indentation++;
            print_statement(loop->body);
            indentation--;

            indentation--;
            break;
        }
        case STATEMENT_CONDITIONAL : {
            Conditional* cond = &statement->conditional;

            u32 if_indent = indentation;
            indented_print("If:\n");
            indentation++;

            mask[if_indent] = true;
            indented_print("Condition:\n");
            indentation++;
            print_expression(cond->condition);
            indentation--;

            if (!cond->false_body) {
                mask[if_indent] = false;
            }

            indented_print("True:\n");
            indentation++;
            print_statement(cond->true_body);
            indentation--;

            if (cond->false_body) {
                mask[if_indent] = false;
                indented_print("False:\n");
                indentation++;
                print_statement(cond->false_body);
                indentation--;
            }

            indentation--;
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
            print_expression(statement->Return.return_expression);
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