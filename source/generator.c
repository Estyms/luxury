#include <generator.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <list.h>
#include <assert.h>

static void generate_statement(Statement* statement);
static void generate_binary_expression(Binary* binary);
static void generate_expression(Expression* expression);

File* file;

void emit(const char* text, ...) {
    static char buffer[1024];

    va_list arg;
    va_start(arg, text);
    u32 size = vsnprintf(buffer, 1024, text, arg);
    va_end(arg);

    fwrite(buffer, 1, size, file);
    fputc('\n', file);
}

u32 stack_level = 0;

void push_rax() {
    emit("    push %%rax");
    stack_level++;
}

void pop_rdi() {
    emit("    pop %%rdi");
    stack_level--;
}

static void generate_address(Expression* expression) {
    if (expression->kind == EXPRESSION_PRIMARY) {
        Primary* primary = &expression->primary;
        if (primary->kind == PRIMARY_IDENTIFIER) {
            assert(primary->declaration);
            emit("    lea %d(%%rbp), %%rax", primary->declaration->variable.offset);
        }
    }
}

static void load_from_rax() {
    emit("    mov (%%rax), %%rax");
}

static void store_to_rax_address() {
    emit("    mov %%rdi, (%%rax)");
}

static void generate_binary_expression(Binary* binary) {
    assert(binary->right);
    assert(binary->left);

    if (binary->kind == BINARY_ASSIGN) {
        generate_expression(binary->right);
        push_rax();
        generate_address(binary->left);
        pop_rdi();
        store_to_rax_address();
        return;
    }

    generate_expression(binary->right);
    push_rax();
    generate_expression(binary->left);
    pop_rdi();

    //   rax
    //    + 
    //   / \
    //  1   2
    // rax rdi
    switch (binary->kind) {
        case BINARY_PLUS : {
            emit("    add %%rdi, %%rax");
            break;
        }
        case BINARY_MINUS : {
            emit("    sub %%rdi, %%rax");
            break;
        }
        case BINARY_MULTIPLICATION : {
            emit("    imul %%rdi, %%rax");
            break;
        }
        case BINARY_DIVISION : {
            emit("    cdq");
            emit("    idiv %%rdi");
            break;
        }
        case BINARY_EQUAL : {
            emit("    cmp %%rdi, %%rax");
            emit("    sete %%al");
            emit("    movzbl %%al, %%eax");
            break;
        }
        case BINARY_LESS : {
            emit("    cmp %%rdi, %%rax");
            emit("    setl %%al");
            emit("    movzbl %%al, %%eax");
            break;
        }
        case BINARY_LESS_EQUAL : {
            emit("    cmp %%rdi, %%rax");
            emit("    setle %%al");
            emit("    movzbl %%al, %%eax");
            break;
        }
        case BINARY_GREATER : {
            emit("    cmp %%rdi, %%rax");
            emit("    setg %%al");
            emit("    movzbl %%al, %%eax");
            break;
        }
        case BINARY_GREATER_EQUAL : {
            emit("    cmp %%rdi, %%rax");
            emit("    setge %%al");
            emit("    movzbl %%al, %%eax");
            break;
        }
        default : {
            printf("error\n");
            exit(1);
        }
    }
}

static void generate_expression(Expression* expression) {
    assert(expression);

    switch (expression->kind) {
        case EXPRESSION_PRIMARY : {
            Primary* primary = &expression->primary;

            if (primary->kind == PRIMARY_NUMBER) {
                emit("    mov $%d, %%rax", primary->number);
            }
            else if (primary->kind == PRIMARY_IDENTIFIER) {
                generate_address(expression);
                load_from_rax();
            }
            else {
                printf("primary expression not handled\n");
                exit(1);
            }
            break;
        }
        case EXPRESSION_UNARY : {
            exit(2134);
            break;
        }
        case EXPRESSION_BINARY : {
            generate_binary_expression(&expression->binary);
            break;
        }
    }
}

Declaration* function_decl;

static void generate_statement(Statement* statement) {
    switch (statement->kind) {
        case STATEMENT_COMPOUND : {
            Compound* compound = &statement->compound;

            ListNode* it;
            list_iterate(it, &compound->statements) {
                Statement* statement = list_to_struct(it, Statement, list_node);
                generate_statement(statement);
            }
            break;
        }
        case STATEMENT_EXPRESSION : {
            generate_expression(statement->expression);
            break;
        }
        case STATEMENT_RETURN : {
            generate_expression(statement->return_statement.return_expression);
            assert(function_decl);
            String name = function_decl->name;
            emit("    jmp end.%.*s\n", name.size, name.text);
            break;
        }
        case STATEMENT_COMMENT : {
            String comment = statement->comment.token->name;
            emit("\n    # %.*s", comment.size, comment.text);
            break;
        }
        default : {
            printf("Generator : statement is not handled\n");
            exit(1);
        }
    }
}

static u32 align(u32 number, u32 alignment) {
    u32 offset = number % alignment;
    if (offset) {
        number = number - offset + alignment;
    }

    return number;
}

static u32 compute_locals_from_scope(Scope* scope, u32 offset) {
    assert(scope);
    ListNode* it;
    list_iterate(it, &scope->child_scopes) {
        Scope* child = list_to_struct(it, Scope, list_node);
        offset = compute_locals_from_scope(child, offset);
    }

    list_iterate(it, &scope->variables) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);

        assert(decl->type);

        offset += decl->type->size;
        offset = align(offset, decl->type->alignment);

        decl->variable.offset = -offset;
    }

    return offset;
}

static u32 compute_local_variable_offset(Function* function) {
    u32 offset = compute_locals_from_scope(function->function_scope, 0);
    return align(offset, 16);
}

static void generate_function(Declaration* declaration) {
    function_decl = declaration;
    Function* function = &declaration->function;

    u32 frame_size = compute_local_variable_offset(function);
    String name = declaration->name;

    emit("    .text");
    emit("    .globl %.*s", name.size, name.text);
    emit("%.*s:", name.size, name.text);
    emit("    push %%rbp");
    emit("    mov %%rsp, %%rbp");
    emit("    sub $%d, %%rsp", frame_size);


    assert(function->body->kind == STATEMENT_COMPOUND);
    generate_statement(function->body);
    assert(stack_level == 0);

    emit("end.%.*s:", name.size, name.text);
    emit("    mov %%rbp, %%rsp");
    emit("    pop %%rbp");
    emit("    ret");
}

static void generate_scope(Scope* scope) {
    ListNode* it;
    list_iterate(it, &scope->functions) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_FUNCTION);
        generate_function(decl);
    }
}

static void generate_code_unit(CodeUnit* code_unit) {
    String name = code_unit->file_name;

    emit("# Code unit : %.*s", name.size, name.text);
    emit("# ------------------------------------------------------\n");

    generate_scope(code_unit->global_scope);
}

void generate_program(Program* program) {
    
    ListNode* it;
    list_iterate(it, &program->code_units) {
        CodeUnit* code_unit = list_to_struct(it, CodeUnit, list_node);

        generate_code_unit(code_unit);
    }
}

void generator_init(const char* output_file) {
    file = fopen(output_file, "w");
    if (file == 0) {
        exit(56);
    }
}