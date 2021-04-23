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

static void generate_binary_expression(Binary* binary) {
    assert(binary->right);
    assert(binary->left);

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
            else {
                printf("Generate expression error\n");
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
        }
    }
}

void generate_program(Statement* statement) {
    generate_statement(statement);

    assert(stack_level == 0);

    emit("    mov %%rbp, %%rsp");
    emit("    ret");
}

void generator_init(const char* output_file) {
    file = fopen(output_file, "w");
    if (file == 0) {
        exit(56);
    }

    emit("    .text");
    emit("    .globl main");
    emit("main:");
    emit("    mov %%rsp, %%rbp");
}