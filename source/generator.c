#include <generator.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <list.h>
#include <assert.h>
#include <error.h>
#include <array.h>

static void generate_statement(Statement* statement);
static void generate_expression(Expression* expression);

const char* argument_registers[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

File* file;
u32 stack_level = 0;
Declaration* current_function_declaration;
Array* data_segment;

void emit(const char* data, ...) {
    static char buffer[1024];

    va_list arg;
    va_start(arg, data);
    u32 size = vsnprintf(buffer, 1024, data, arg);
    va_end(arg);

    fwrite(buffer, 1, size, file);
    fputc('\n', file);
}

void emit_data(const char* data, ...) {
    va_list arg;
    va_start(arg, data);
    array_add_va_list(data_segment, data, arg);
    va_end(arg);

    array_add(data_segment, "\n");
}

void emit_data_segment() {
    if (data_segment->size == 0) {
        return;
    }

    emit("");
    emit("    .data");

    fwrite(data_segment->buffer, 1, data_segment->size, file);
    data_segment->size = 0;
}

void push_rax() {
    emit("    push %%rax");
    stack_level++;
}

void pop_rdi() {
    emit("    pop %%rdi");
    stack_level--;
}

void pop(const char* reg) {
    emit("    pop %%%s", reg);
    stack_level--;
}

static void generate_address(Expression* expression) {
    if (is_variable(expression)) {
        assert(expression->primary.declaration);

        if (expression->primary.declaration->is_global) {
            String name = expression->primary.declaration->name;
            emit("    lea %.*s, %%rax", name.size, name.text);
        }
        else {
            emit("    lea %d(%%rbp), %%rax", expression->primary.declaration->variable.offset);
        }
    }
    else if (is_deref(expression)) {
        generate_expression(expression->unary.operand);
    }
    else if (expression->kind == EXPRESSION_DOT) {
        generate_address(expression->dot.expression);
        emit("    add $%d, %%rax", expression->dot.offset);
    }
    else {
        printf("Generator (address) : cannot generate address of this.\n");
        exit(1);
    }
}

static void load_from_rax(Type* type) {
    assert(type);

    if (type->kind == TYPE_POINTER && type->pointer.count) {
        return;
    }

    switch (type->size) {
        case 1 : emit("    movsbq (%%rax), %%rax"); break;
        case 2 : emit("    movswq   (%%rax), %%rax"); break;
        case 4 : emit("    movslq   (%%rax), %%rax"); break;
        case 8 : emit("    movq   (%%rax), %%rax"); break;
    }
}

static void store_to_rax_address(Type* type) {
    switch (type->size) {
        case 1 : emit("    movb %%dil, (%%rax)"); break;
        case 2 : emit("    movw %%di, (%%rax)"); break;
        case 4 : emit("    movl %%edi, (%%rax)"); break;
        case 8 : emit("    movq %%rdi, (%%rax)"); break;
    }
}

static void generate_binary_expression(Expression* expression) {
    Binary* binary = &expression->binary;

    assert(binary->right);
    assert(binary->left);

    if (binary->kind == BINARY_ASSIGN) {
        generate_expression(binary->right);
        push_rax();
        
        generate_address(binary->left);
        pop_rdi();
        store_to_rax_address(expression->type);
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
            emit("    movzb %%al, %%eax");
            break;
        }
        case BINARY_LESS : {
            emit("    cmp %%rdi, %%rax");
            emit("    setl %%al");
            emit("    movzb %%al, %%eax");
            break;
        }
        case BINARY_LESS_EQUAL : {
            emit("    cmp %%rdi, %%rax");
            emit("    setle %%al");
            emit("    movzb %%al, %%eax");
            break;
        }
        case BINARY_GREATER : {
            emit("    cmp %%rdi, %%rax");
            emit("    setg %%al");
            emit("    movzb %%al, %%eax");
            break;
        }
        case BINARY_GREATER_EQUAL : {
            emit("    cmp %%rdi, %%rax");
            emit("    setge %%al");
            emit("    movzb %%al, %%eax");
            break;
        }
        default : {
            printf("error\n");
            exit(1);
        }
    }
}

static void generate_primary_expression(Expression* expression) {
    Primary* primary = &expression->primary;

    switch (primary->kind) {
        case PRIMARY_NUMBER : {
            emit("    mov $%d, %%rax", primary->number);
            break;
        }
        case PRIMARY_IDENTIFIER : {
            generate_address(expression);
            load_from_rax(expression->type);
            break;
        }
        case PRIMARY_STRING : {
            static u32 string_number = 0;

            // Just emit the string.
            emit_data("string.%d:", string_number);
            emit_data("    .string \"%.*s\"", primary->string.size, primary->string.text);

            emit("    lea string.%d, %%rax", string_number);

            string_number++;
            break;
        }
        default : {
            printf("Generator : primary expression not handled %d \n", primary->kind);
            exit(1);
        }
    }
}

static void generate_unary_expression(Expression* expression) {
    Unary* unary = &expression->unary;

    if (unary->kind == UNARY_DEREF) {
        generate_expression(unary->operand);
        load_from_rax(expression->type);
    }
    else if (unary->kind == UNARY_ADDRESS_OF) {
        generate_address(unary->operand);
    }
    else {
        printf("Generator : unary expression is not handled\n");
        exit(1);
    }
}

static void generate_call_expression(Expression* expression) {
    Call* call = &expression->call;

    u32 argument_count = 0;
    ListNode* it;
    list_iterate(it, &call->arguments) {
        Expression* expr = list_to_struct(it, Expression, list_node);
        generate_expression(expr);
        push_rax();
        argument_count++;
    }

    while(argument_count--) {
        pop(argument_registers[argument_count]);
    }

    emit("    mov $0, %%rax");

    String name = call->expression->primary.name;
    emit("    call %.*s", name.size, name.text);
}

static void generate_dot_expression(Expression* expression) {
    generate_address(expression);
    load_from_rax(expression->type);
}

static void generate_expression(Expression* expression) {
    assert(expression);

    switch (expression->kind) {
        case EXPRESSION_PRIMARY : {
            generate_primary_expression(expression);
            break;
        }
        case EXPRESSION_UNARY : {
            generate_unary_expression(expression);
            break;
        }
        case EXPRESSION_BINARY : {
            generate_binary_expression(expression);
            break;
        }
        case EXPRESSION_CALL : {
            generate_call_expression(expression);
            break;
        }
        case EXPRESSION_DOT : {
            generate_dot_expression(expression);
            break;
        }
        default : {
            printf("Generator : expression kind is not handled\n");
            exit(1);
        }
    }
}

static void generate_compound_statement(Statement* statement) {
    Compound* compound = &statement->compound;

    ListNode* it;
    list_iterate(it, &compound->statements) {
        Statement* statement = list_to_struct(it, Statement, list_node);
        generate_statement(statement);
    }
}

static void generate_return_statement(Statement* statement) {
    generate_expression(statement->Return.return_expression);
    assert(current_function_declaration);
    String name = current_function_declaration->name;
    emit("    jmp end.%.*s", name.size, name.text);
}

static void generate_loop_statement(Statement* statement) {
    static u32 loop_counter = 0;

    Loop* loop = &statement->loop;
    u32 number = loop_counter++;

    if (loop->init_statement) {
        generate_statement(loop->init_statement);
    }
    emit("loop.start.%d:", number);

    generate_expression(loop->condition);
    emit("    cmp $0, %%rax");
    emit("    je loop.end.%d", number);

    generate_statement(loop->body);

    if (loop->post_statement) {
        generate_statement(loop->post_statement);
    }
    emit("    jmp loop.start.%d", number);

    emit("loop.end.%d:", number);
}

static void generate_conditional_statement(Statement* statement) {
    static u32 if_counter = 0;

    Conditional* cond = &statement->conditional;
    u32 number = if_counter++;

    generate_expression(cond->condition);
    emit("    cmp $0, %%rax");
    emit("    je if.false.%d", number);
    generate_statement(cond->true_body);
    emit("    jmp if.end.%d", number);
    
    emit("if.false.%d:", number);
    if (cond->false_body) {
        generate_statement(cond->false_body);
    }

    emit("if.end.%d:", number);
}

static void generate_comment_statement(Statement* statement) {
    String comment = statement->comment.token->name;
    emit("\n    # %.*s", comment.size, comment.text);
}

static void generate_statement(Statement* statement) {
    switch (statement->kind) {
        case STATEMENT_COMPOUND : {
            generate_compound_statement(statement);
            break;
        }
        case STATEMENT_EXPRESSION : {
            generate_expression(statement->expression);
            break;
        }
        case STATEMENT_RETURN : {
            generate_return_statement(statement);
            break;
        }
        case STATEMENT_LOOP : {
            generate_loop_statement(statement);
            break;
        }
        case STATEMENT_CONDITIONAL : {
            generate_conditional_statement(statement);
            break;
        }
        case STATEMENT_COMMENT : {
            generate_comment_statement(statement);
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
        String name = decl->name;
        //printf("assigning stack : %.*s with size %d\n", name.size, name.text, decl->type->size);

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
    current_function_declaration = declaration;
    Function* function = &declaration->function;

    u32 frame_size = compute_local_variable_offset(function);
    String name = declaration->name;

    emit("");
    emit("    .text");
    emit("    .globl %.*s", name.size, name.text);
    emit("%.*s:", name.size, name.text);
    emit("    push %%rbp");
    emit("    mov %%rsp, %%rbp");
    emit("    sub $%d, %%rsp", frame_size);

    // Store the argument registers on the assigned place on the stack frame.
    u32 reg = 0;
    ListNode* it;
    list_iterate(it, &function->function_scope->variables) {
        if (reg >= 6) {
            error_token(declaration->name_token, "this function uses more than 6 arguments");
        }

        Declaration* decl = list_to_struct(it, Declaration, list_node);
        emit("    mov %%%s, %d(%%rbp)", argument_registers[reg++], decl->variable.offset);
    }

    assert(function->body->kind == STATEMENT_COMPOUND);
    generate_statement(function->body);
    assert(stack_level == 0);

    emit("end.%.*s:", name.size, name.text);
    emit("    mov %%rbp, %%rsp");
    emit("    pop %%rbp");
    emit("    ret");

    emit_data_segment();
}

static void generate_scope(Scope* scope) {
    ListNode* it;
    list_iterate(it, &scope->functions) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_FUNCTION);
        generate_function(decl);
    }

    if (scope->parent == 0) {
        // Global scope.
        ListNode* it;
        list_iterate(it, &scope->variables) {
            Declaration* declaration = list_to_struct(it, Declaration, list_node);

            emit_data("%.*s:", declaration->name.size, declaration->name.text);
            emit_data("    .zero %d", declaration->type->size);
        }
    }

    emit_data_segment();
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

    data_segment = new_array();
}