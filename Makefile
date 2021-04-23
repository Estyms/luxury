top    = $(shell pwd)
build  = $(top)/build
target = $(build)/luxury
test   = main.lux

source += source/str.c
source += source/main.c
source += source/lexer.c
source += source/compiler_error.c
source += source/parser.c
source += source/tree.c
source += source/tree_printer.c
source += source/generator.c

include += include/list.h
include += include/str.h
include += include/types.h
include += include/lexer.h
include += include/typedef.h
include += include/compiler_error.h
include += include/parser.h
include += include/tree.h
include += include/tree_printer.h
include += include/generator.h

flags += -Wno-unused-function -Wall -std=c11 -g -Wno-comment
flags += -Wno-switch -fno-common -Wno-unused-variable -Wno-return-type

source_global  = $(addprefix $(top)/, $(source))
include_global = $(addprefix $(top)/, $(include))
test_file      = $(addprefix $(top)/test/, $(test))

all: luxury
	@build/luxury $(test_file) $(build)/output.s
	@gcc -static -o $(build)/output $(build)/output.s
	@$(build)/output

compile: 
	@gcc -static -o $(build)/output $(build)/output.s
	@$(build)/output	

luxury: $(include_global)
	@mkdir -p $(build)
	@$(CC) -Iinclude $(source_global) $(flags) -o $(target)

count: 
	@cloc -no3 --by-file source/*.c

clean: 
	@rm -r -f $(build)