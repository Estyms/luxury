#include <str.h>
#include <types.h>

bool string_compare(String* a, String* b) {
    if (a->size != b->size) {
        return false;
    }

    for (u32 i = 0; i < a->size; i++) {
        if (a->text[i] != b->text[i]) {
            return false;
        }
    }

    return true;
}
