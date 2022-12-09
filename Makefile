OBJECTS  = ./build/compiler.o ./build/cprocess.o \
           ./build/lexer.o ./build/lex_process.o ./build/token.o \
           ./build/parser.o ./build/node.o ./build/expressionable.o ./build/datatype.o ./build/scope.o ./build/symresolver.o ./build/resolver.o ./build/rdefault.o ./build/codegen.o ./build/stackframe.o ./build/fixup.o ./build/array.o ./build/helper.o \
           ./build/preprocessor.o \
           ./build/helpers/buffer.o ./build/helpers/vector.o
INCLUDES = -I./
CFLAGS   = -g -Wall -Wno-unused-variable -Wno-unused-function

all: ./main

./main: main.c ${OBJECTS}
	gcc main.c ${INCLUDES} ${OBJECTS} ${CFLAGS} -o ./main

./build/compiler.o: ./compiler.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./compiler.c -o ./build/compiler.o

./build/cprocess.o: ./cprocess.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./cprocess.c -o ./build/cprocess.o

./build/lexer.o: ./lexer.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./lexer.c -o ./build/lexer.o

./build/lex_process.o: ./lex_process.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./lex_process.c -o ./build/lex_process.o

./build/token.o: ./token.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./token.c -o ./build/token.o

./build/parser.o: ./parser.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./parser.c -o ./build/parser.o

./build/node.o: ./node.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./node.c -o ./build/node.o

./build/expressionable.o: ./expressionable.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./expressionable.c -o ./build/expressionable.o

./build/datatype.o: ./datatype.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./datatype.c -o ./build/datatype.o

./build/scope.o: ./scope.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./scope.c -o ./build/scope.o

./build/symresolver.o: ./symresolver.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./symresolver.c -o ./build/symresolver.o

./build/resolver.o: ./resolver.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./resolver.c -o ./build/resolver.o

./build/rdefault.o: ./rdefault.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./rdefault.c -o ./build/rdefault.o

./build/codegen.o: ./codegen.c ./compiler.h
	gcc ${INCLUDES} ${CFLAGS} -c ./codegen.c -o ./build/codegen.o

./build/stackframe.o: ./stackframe.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./stackframe.c -o ./build/stackframe.o

./build/fixup.o: ./fixup.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./fixup.c -o ./build/fixup.o

./build/array.o: ./array.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./array.c -o ./build/array.o

./build/helper.o: ./helper.c ./compiler.h ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./helper.c -o ./build/helper.o

./build/preprocessor.o: ./preprocessor/preprocessor.c ./compiler.h ./helpers/vector.h ./helpers/buffer.h
	gcc ${INCLUDES} ${CFLAGS} -c ./preprocessor/preprocessor.c -o ./build/preprocessor.o

./build/helpers/buffer.o: ./helpers/buffer.c ./helpers/buffer.h
	gcc ${INCLUDES} ${CFLAGS} -c ./helpers/buffer.c -o ./build/helpers/buffer.o

./build/helpers/vector.o: ./helpers/vector.c ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./helpers/vector.c -o ./build/helpers/vector.o

clean:
	rm -f ./main
	rm -rf ./build
	mkdir -p ./build/helpers

.PHONY: all clean
