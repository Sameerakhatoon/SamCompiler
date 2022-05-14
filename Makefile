OBJECTS  = ./build/compiler.o ./build/cprocess.o \
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

./build/helpers/buffer.o: ./helpers/buffer.c ./helpers/buffer.h
	gcc ${INCLUDES} ${CFLAGS} -c ./helpers/buffer.c -o ./build/helpers/buffer.o

./build/helpers/vector.o: ./helpers/vector.c ./helpers/vector.h
	gcc ${INCLUDES} ${CFLAGS} -c ./helpers/vector.c -o ./build/helpers/vector.o

clean:
	rm -f ./main
	rm -rf ./build
	mkdir -p ./build/helpers

.PHONY: all clean
