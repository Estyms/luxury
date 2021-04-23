#include <stdio.h>
#include <assert.h>
#include <str.h>
#include <stdlib.h>
#include <lexer.h>
#include <parser.h>
#include <tree_printer.h>
#include <generator.h>

void read_source_file(String* string, char* file_name) {
    File* file = fopen(file_name, "r");
    if (!file) {
        exit(1);
    }
    
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

int main(int argument_count, char** arguments) {
    assert(argument_count == 3);
    printf("Compiling : %s\n", arguments[1]);
    printf("Output    : %s\n", arguments[2]);

    String source_file;
    String source_file_name;
    
    read_source_file(&source_file, arguments[1]);


    Lexer* lexer = new_lexer(&source_file, &source_file_name);
    Parser* parser = new_parser(lexer);
    Statement* statement = parse_statement(parser);
    print_statement(statement);

    generator_init(arguments[2]);
    generate_program(statement);
}