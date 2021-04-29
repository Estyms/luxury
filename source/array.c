#include <array.h>
#include <stdlib.h>

const u32 INITIAL_SIZE = 1024;

Array* new_array() {
    Array* array = malloc(sizeof(Array));

    array->buffer = malloc(INITIAL_SIZE);
    array->capacity = INITIAL_SIZE;
    array->size = 0;

    return array;
}

static inline void array_add_char(Array* array, char c) {
    if (array->size >= array->capacity) {
        // Reallocate a bigger buffer.
        char* new_buffer = malloc(array->capacity * 2);

        if (new_buffer == 0) {
            printf("Array : malloc failed\n");
            exit(1);
        }

        for (u32 i = 0; i < array->size; i++) {
            new_buffer[i] = array->buffer[i];
        }

        free(array->buffer);

        array->buffer    = new_buffer;
        array->capacity *= 2;
    }

    // Push the data.
    array->buffer[array->size++] = c;
}

void array_add(Array* array, const char* data) {
    while (*data) {
        array_add_char(array, *data++);
    }
}

void array_add_va_list(Array* array, const char* data, va_list arg) {
    static char buffer[2048];
    u32 size = vsnprintf(buffer, 2048, data, arg);

    char* pointer = buffer;
    while (size--) {
        array_add_char(array, *pointer++);
    }
}

void array_add_format(Array* array, const char* data, ...) {
    va_list arg;
    va_start(arg, data);

    array_add_va_list(array, data, arg);

    va_end(arg);
}
