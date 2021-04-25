#ifndef TYPER_H
#define TYPER_H

#include <types.h>
#include <tree.h>
#include <typedef.h>

extern Type* type_u64;
extern Type* type_u32;
extern Type* type_u16;
extern Type* type_u8;
extern Type* type_s64;
extern Type* type_s32;
extern Type* type_s16; 
extern Type* type_s8;
extern Type* type_char;

struct Typer {
    Scope* current_scope;  
};

void type_program(Program* programs, Typer* typer);

#endif