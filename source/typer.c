#include <typer.h>
#include <stdio.h>
#include <tree_printer.h>
#include <string.h>
#include <assert.h>
#include <error.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>

static void type_scope(Scope* scope, Typer* typer);
static void type_code_unit(CodeUnit* code_unit, Typer* typer);
static void type_function(Declaration* decl, Typer* typer);
static void type_statement(Statement* statement, Typer* typer);
static void type_expression(Expression* expression, Typer* typer);
static void type_binary_expression(Expression* expression, Typer* typer);
static void type_unary_expression(Expression* expression, Typer* typer);
static void type_primary_expression(Expression* expression, Typer* typer);
static void type_call_expression(Expression* expression, Typer* typer);
static Type* resolve_type(Type* type, Typer* typer);

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

static bool is_valid_type(Type* type) {
    return type->kind != TYPE_UNKNOWN && type->kind != TYPE_INFERRED;
}

static bool is_pointer(Expression* expression) {
    assert(expression->type);
    return expression->type->kind == TYPE_POINTER;
}

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
    if (expression->type) {
        return;
    }

    Binary* binary = &expression->binary;
    assert(binary->left);
    assert(binary->right);

    type_expression(binary->right, typer);
    type_expression(binary->left, typer);

    if (binary->right->type == 0) {
        typer->unresolved_types = true;
        return;
    }

    // Handle the type inferrence. If the right hand side type is known, we type the left hand side,
    // if not, we return.
    if (is_inferred(binary->left)) {
        if (binary->right->type == 0) {
            typer->unresolved_types = true;
            return;
        }

        binary->left->primary.declaration->type = binary->right->type;
        binary->left->type = binary->right->type;
        String name = binary->left->primary.declaration->name;
        printf("Inferring : %.*s\n", name.size, name.text);
    }
    else if (binary->left->type == 0) {
        typer->unresolved_types = true;
        return;
    }

    assert(binary->left->type);
    assert(binary->right->type);

    expression->type = binary->left->type;

    if (binary->kind == BINARY_PLUS) {

        if (!is_pointer(binary->left) && !is_pointer(binary->right)) {
            expression->type = binary->left->type;
            return;
        }

        if (is_pointer(binary->left) && is_pointer(binary->right)) {
            error_token(binary->operator, "cannot use this operator on two pointers");
        }

        if (!is_pointer(binary->left) && is_pointer(binary->right)) {
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
    if (expression->type) {
        return;
    }

    assert(expression);
    assert(typer);

    Unary* unary = &expression->unary;
    assert(unary->operand);

    type_expression(unary->operand, typer);

    if (unary->operand->type == 0) {
        typer->unresolved_types = true;
        return;
    }

    if (unary->kind == UNARY_ADDRESS_OF) {
        Type* type = new_pointer();
        type->pointer.pointer_to = unary->operand->type;
        expression->type = type;
    }
    else if (unary->kind == UNARY_DEREF) {
        expression->type = unary->operand->type->pointer.pointer_to;
    }
}

static void type_primary_expression(Expression* expression, Typer* typer) {
    if (expression->type) {
        return;
    }

    Primary* primary = &expression->primary;

    if (primary->kind == PRIMARY_IDENTIFIER) {
        if (primary->declaration == 0) {
            Declaration* decl = lookup_in_current_scope(typer, &primary->name, DECLARATION_VARIABLE);
            if (decl == 0) {
                error_token(primary->token, "variables is not declarred");
            }
            
            primary->declaration = decl;
            assert(decl->type);
        }  

        if (is_valid_type(primary->declaration->type)) {
            expression->type = primary->declaration->type;
            typer->type_resolved = true;
        }
        else {
            typer->unresolved_types = true;
        }
    }
    else if (primary->kind == PRIMARY_NUMBER) {
        expression->type = type_u64;
        typer->type_resolved = true;
    }
    else if (primary->kind == PRIMARY_STRING) {
        Type* type = new_pointer();
        type->pointer.pointer_to = type_char;
        expression->type = type;
        typer->type_resolved = true;
    }
}

static void type_call_expression(Expression* expression, Typer* typer) {
    if (expression->type) {
        return;
    }

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
        typer->type_resolved = true;
        expression->type = decl->function.return_type;
    }
}

static StructMember* lookup_member_in_struct(String* name, Type* type) {
    assert(type->kind == TYPE_STRUCT);
    assert(type->Struct.scope);

    ListNode* it;
    list_iterate(it, &type->Struct.scope->members) {
        StructMember* member = list_to_struct(it, StructMember, scope_node);

        assert(member->is_anonymous == false);

        if (string_compare(&member->name, name)) {
            return member;
        }
    }

    return 0;
}

static void type_dot_expression(Expression* expression, Typer* typer) {
    if (expression->type) {
        return;
    }

    Dot* dot = &expression->dot;

    if (dot->expression->type == 0) {
        type_expression(dot->expression, typer);
    }

    if (dot->expression->type) {
        while (dot->expression->type->kind == TYPE_POINTER) {
            Type* type = dot->expression->type->pointer.pointer_to;

            // We are having a struct member of something which is a pointer.
            Unary* unary = new_unary(UNARY_DEREF);
            unary->operand = dot->expression;
            dot->expression = (Expression *)unary;  
            dot->expression->type = type;

            type_unary_expression((Expression *)unary, typer);
        }

        StructMember* member = lookup_member_in_struct(&dot->member->name, dot->expression->type);

        if (member == 0) {
            error_token(dot->member, "invalid struct member");
        }

        typer->type_resolved = true;
        dot->offset = member->offset;
        expression->type = member->type;
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
        case EXPRESSION_DOT : {
            type_dot_expression(expression, typer);
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
    if (loop->init_statement) {
        type_statement(loop->init_statement, typer);
    }

    type_expression(loop->condition, typer);

    if (loop->post_statement) {
        type_statement(loop->post_statement, typer);
    }

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

    if (function->assembly_function == false) {
        assert(function->body->compound.scope != function->function_scope);
        type_scope(function->body->compound.scope, typer);
        type_statement(function->body, typer);
    }

    exit_scope(typer);
}

static Type* resolve_unknown_type(Type* type, Typer* typer) {
    UnknownType* unknown = &type->unknown;

    String name = unknown->token->name;
    Declaration* declaration = lookup_in_current_scope(typer, &name, DECLARATION_TYPE);
    if (declaration) {
        assert(declaration->type);

        if (is_valid_type(declaration->type)) {
            typer->type_resolved = true;
            return declaration->type;
        }
        else {
            typer->unresolved_types = true;
            return type;
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

static void compute_struct_offsets(Type* type) {
    assert(type->kind == TYPE_STRUCT);
    StructType* Struct = &type->Struct;

    u32 offset = 0;
    u32 alignment = 0;
    u32 size = 0;

    ListNode* it;
    list_iterate(it, &Struct->members) {
        StructMember* member = list_to_struct(it, StructMember, list_node);
        Type* member_type = member->type;

        if (member->type->kind == TYPE_STRUCT) {
            // This will compute the size and alignment of the sub-structure.
            compute_struct_offsets(member->type);
        }

        assert(member->type);
        assert(member->type->size);

        if (Struct->is_struct) {
            // Structure.
            offset = align(offset, member_type->size);
            size += member_type->size;
            member->offset = offset;
            offset += member_type->size;
        }
        else {
            // Union.
            member->offset = 0;

            if (member_type->size > size) {
                size = member_type->size;
            }
        }


        if (member_type->alignment > alignment) {
            alignment = member_type->alignment;
        }
    }

    type->alignment = alignment;
    type->size = align(size, alignment);
}

static void fix_struct_offsets(Type* type, u32 offset) {
    assert(type->kind == TYPE_STRUCT);
    StructType* Struct = &type->Struct;

    if (Struct->scope) {
        offset = 0;
    }

    ListNode* it;
    list_iterate(it, &Struct->members) {
        StructMember* member = list_to_struct(it, StructMember, list_node);
        Type* member_type = member->type;

        member->offset += offset;

        if (member_type->kind == TYPE_STRUCT) {
            fix_struct_offsets(member->type, member->offset);
        }
    }
}

static Type* resolve_struct_type(Type* type, Typer* typer) {
    assert(type->kind == TYPE_STRUCT);
    assert(type->Struct.scope);

    StructScope* scope = type->Struct.scope;
    if (scope->typing_complete == true) {
        return type;
    }

    ListNode* it;
    list_iterate(it, &type->Struct.scope->members) {
        StructMember* member = list_to_struct(it, StructMember, scope_node);

        if (member->is_anonymous) {
            continue;
        }

        member->type = resolve_type(member->type, typer);
    }

    return type;
}

static Type* resolve_type(Type* type, Typer* typer) {
    switch (type->kind) {
        case TYPE_POINTER : {
            type->pointer.pointer_to = resolve_type(type->pointer.pointer_to, typer);
            break;
        }
        case TYPE_INFERRED : {
            break;
        }
        case TYPE_UNKNOWN : {
            return resolve_unknown_type(type, typer);
            break;
        }
        case TYPE_STRUCT : {
            return resolve_struct_type(type, typer);
            break;
        }
        case TYPE_BASIC : {
            break;
        }
        default : {
            printf("Typer : unknown type kind %d\n", type->kind);
            exit(1);
        }
    }

    return type;
}

static void resolve_declraration_type(Declaration* declaration, Typer* typer) {
    // We are using one global variable to determine if there are any unresolved types still.
    // We save that variable (restore it after), to check if the entire structure is still
    // untyped.
    bool saved_unresolved = typer->unresolved_types;
    typer->unresolved_types = false;

    if (declaration->type->kind != TYPE_STRUCT || declaration->type->Struct.scope->typing_complete == false) {
        declaration->type = resolve_type(declaration->type, typer);

        if (declaration->type->kind == TYPE_STRUCT) {
            declaration->type->Struct.scope->typing_complete = !typer->unresolved_types;

            if (declaration->type->Struct.scope->typing_complete) {
                compute_struct_offsets(declaration->type);
                fix_struct_offsets(declaration->type, 0);
            }
        }
    }

    typer->unresolved_types = typer->unresolved_types || saved_unresolved;
}

static void type_scope(Scope* scope, Typer* typer) {
    enter_scope(typer, scope);

    ListNode* it;
    list_iterate(it, &scope->types) {
        Declaration* declaration = list_to_struct(it, Declaration, list_node);
        resolve_declraration_type(declaration, typer);        
    }

    list_iterate(it, &scope->variables) {        
        Declaration* declaration = list_to_struct(it, Declaration, list_node);
        resolve_declraration_type(declaration, typer);
    }

    list_iterate(it, &scope->functions) {
        Declaration* decl = list_to_struct(it, Declaration, list_node);
        assert(decl->kind == DECLARATION_FUNCTION);

        if (decl->function.return_type == 0) {
            decl->function.return_type = new_type(TYPE_VOID);
        }

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

        while (typer->unresolved_types && typer->type_resolved) {
            typer->type_resolved = false;
            typer->unresolved_types = false;
            type_code_unit(code_unit, typer);
        }

        if (typer->unresolved_types) {
            printf("Typer failed\n");
            exit(1);
        }
    }
}
