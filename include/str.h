#ifndef STRING_H
#define STRING_H

#include <types.h>
#include <typedef.h>

struct String {
    char* text;
    u32 size;
};

bool string_compare(String* a, String* b);

#endif
