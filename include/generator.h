#ifndef GENERATOR_H
#define GENERATOR_H

#include <types.h>
#include <tree.h>

void generator_init(const char* output_file);
void generate_program(Program* program);

#endif
