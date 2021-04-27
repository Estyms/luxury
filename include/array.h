#ifndef ARRAY_H
#define ARRAY_H

#include <types.h>
#include <typedef.h>
#include <stdarg.h>

struct Array {
    char* buffer;
    u32 capacity;
    u32 size;
};

Array* new_array();

void array_add(Array* array, const char* data);
void array_add_va_list(Array* array, const char* data, va_list arg);
void array_add_format(Array* array, const char* data, ...);

#endif