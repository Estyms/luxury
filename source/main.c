#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <lexer.h>
#include <parser.h>
#include <tree_printer.h>
#include <generator.h>
#include <typer.h>

void read_source_file(String* string, char* file_name) {
    File* file = fopen(file_name, "r");
    if (!file) {
        exit(1);
    }

    printf("size of file is : %ld\n", sizeof(File));
    
    fseek(file, 0, SEEK_END);
    u32 file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    string->size = file_size + 1;
    string->text = malloc(string->size);

    assert(fread(string->text, 1, file_size, file) == file_size);
    string->text[file_size] = 0;
}

void print_token(Token* token) {
    printf("%.*s\n", token->name.size, token->name.text);
}

static u32 string_length(const char* data) {
    u32 length = 0;
    while (*data++) {
        length++;
    }

    return length;
}

int main(int argument_count, char** arguments) {
    assert(argument_count == 3);

    printf("Input file  : %s\n", arguments[1]);
    printf("Output file : %s\n", arguments[2]);

    String source_file;
    String source_file_name = (String){ .text = arguments[1], .size = string_length(arguments[1]) };

    // We read the entire source file into memory.
    read_source_file(&source_file, arguments[1]);

    Lexer* lexer   = new_lexer(&source_file, &source_file_name);
    Parser* parser = new_parser(lexer);
    Typer typer;
    
    // Build the syntax tree.
    Program* program = parser_program(parser);
    print_program(program);

    printf("Typing starting\n");

    // Type the syntax tree.
    type_program(program, &typer);
    print_program(program);

    // Generate the output file from the typed syntax tree.
    generator_init(arguments[2]);
    generate_program(program);
}