OBJECTS  = ./build/compiler.o ./build/cprocess.o ./build/validator.o \
           ./build/lexer.o ./build/lex_process.o ./build/token.o \
           ./build/parser.o ./build/node.o ./build/expressionable.o ./build/datatype.o ./build/scope.o ./build/symresolver.o ./build/resolver.o ./build/rdefault.o ./build/codegen.o ./build/codegen_x86.o ./build/codegen_x86_64.o ./build/codegen_aarch64.o ./build/codegen_riscv64.o ./build/target.o ./build/stackframe.o ./build/fixup.o ./build/array.o ./build/initializer.o ./build/exp64.o ./build/helper.o ./build/constant.o ./build/atomic.o \
           ./build/preprocessor.o ./build/native.o \
           ./build/static-include.o ./build/static-includes/stdarg.o ./build/static-includes/stddef.o \
           ./build/helpers/buffer.o ./build/helpers/vector.o ./build/helpers/arena.o \
           ./build/os.o
# -g.
OBJECTS += ./build/debuginfo.o
INCLUDES = -I./
# -Wall -Wextra with nothing hidden except unused parameters: callbacks
# such as the expressionable and static-include tables must take the
# full signature even when they ignore an argument.
CFLAGS   = -g -Wall -Wextra -Wno-unused-parameter
# -MMD writes a .d file next to each object listing every header it read,
# so a header edit rebuilds exactly the objects that include it.
DEPFLAGS = -MMD -MP

all: ./main

./main: main.c ${OBJECTS}
	gcc main.c ${INCLUDES} ${OBJECTS} ${CFLAGS} ${DEPFLAGS} -MF ./build/main.d -o ./main

# A flag change in this file must rebuild everything.
./main ${OBJECTS}: Makefile

./build/compiler.o: ./compiler.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./compiler.c -o ./build/compiler.o

./build/cprocess.o: ./cprocess.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./cprocess.c -o ./build/cprocess.o

./build/validator.o: ./validator.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./validator.c -o ./build/validator.o

./build/lexer.o: ./lexer.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./lexer.c -o ./build/lexer.o

./build/lex_process.o: ./lex_process.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./lex_process.c -o ./build/lex_process.o

./build/token.o: ./token.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./token.c -o ./build/token.o

./build/constant.o: ./constant.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./constant.c -o ./build/constant.o

./build/parser.o: ./parser.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./parser.c -o ./build/parser.o

./build/node.o: ./node.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./node.c -o ./build/node.o

./build/expressionable.o: ./expressionable.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./expressionable.c -o ./build/expressionable.o

./build/datatype.o: ./datatype.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./datatype.c -o ./build/datatype.o

./build/scope.o: ./scope.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./scope.c -o ./build/scope.o

./build/symresolver.o: ./symresolver.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./symresolver.c -o ./build/symresolver.o

./build/resolver.o: ./resolver.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./resolver.c -o ./build/resolver.o

./build/rdefault.o: ./rdefault.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./rdefault.c -o ./build/rdefault.o

./build/codegen.o: ./codegen.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./codegen.c -o ./build/codegen.o

./build/codegen_x86.o: ./codegen_x86.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./codegen_x86.c -o ./build/codegen_x86.o

./build/codegen_x86_64.o: ./codegen_x86_64.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./codegen_x86_64.c -o ./build/codegen_x86_64.o

./build/codegen_aarch64.o: ./codegen_aarch64.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./codegen_aarch64.c -o ./build/codegen_aarch64.o

./build/codegen_riscv64.o: ./codegen_riscv64.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./codegen_riscv64.c -o ./build/codegen_riscv64.o

./build/target.o: ./target.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./target.c -o ./build/target.o

./build/stackframe.o: ./stackframe.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./stackframe.c -o ./build/stackframe.o

./build/fixup.o: ./fixup.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./fixup.c -o ./build/fixup.o

./build/array.o: ./array.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./array.c -o ./build/array.o

./build/initializer.o: ./initializer.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./initializer.c -o ./build/initializer.o

./build/exp64.o: ./exp64.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./exp64.c -o ./build/exp64.o

./build/atomic.o: ./atomic.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./atomic.c -o ./build/atomic.o

./build/debuginfo.o: ./debuginfo.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./debuginfo.c -o ./build/debuginfo.o

./build/helper.o: ./helper.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./helper.c -o ./build/helper.o

./build/os.o: ./os.c ./os.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./os.c -o ./build/os.o

./build/preprocessor.o: ./preprocessor/preprocessor.c ./compiler.h ./helpers/vector.h ./helpers/buffer.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./preprocessor/preprocessor.c -o ./build/preprocessor.o

./build/native.o: ./preprocessor/native.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./preprocessor/native.c -o ./build/native.o

./build/static-include.o: ./preprocessor/static-include.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./preprocessor/static-include.c -o ./build/static-include.o

./build/static-includes/stdarg.o: ./preprocessor/static-includes/stdarg.c ./compiler.h
	mkdir -p ./build/static-includes
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./preprocessor/static-includes/stdarg.c -o ./build/static-includes/stdarg.o

./build/static-includes/stddef.o: ./preprocessor/static-includes/stddef.c ./compiler.h
	mkdir -p ./build/static-includes
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./preprocessor/static-includes/stddef.c -o ./build/static-includes/stddef.o

./build/helpers/buffer.o: ./helpers/buffer.c ./helpers/buffer.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./helpers/buffer.c -o ./build/helpers/buffer.o

./build/helpers/vector.o: ./helpers/vector.c ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./helpers/vector.c -o ./build/helpers/vector.o

./build/helpers/arena.o: ./helpers/arena.c ./helpers/arena.h
	gcc ${INCLUDES} ${CFLAGS} ${DEPFLAGS} -c ./helpers/arena.c -o ./build/helpers/arena.o

clean:
	rm -f ./main
	rm -rf ./build
	mkdir -p ./build/helpers

.PHONY: all clean

-include ./build/main.d $(OBJECTS:.o=.d)
