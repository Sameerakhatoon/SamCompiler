#include "buffer.h"
#include "arena.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// The lexer makes a buffer for every identifier, number and operator it
// reads, and each started at 2000 bytes: 440 MB for a file of 20,000
// small functions. Buffers now start small and grow geometrically.
#define BUFFER_INITIAL_SIZE 32

struct buffer* buffer_create(){
    struct buffer* buf = calloc(sizeof(struct buffer), 1);
    buf->data  = calloc(BUFFER_INITIAL_SIZE, 1);
    buf->len   = 0;
    buf->msize = BUFFER_INITIAL_SIZE;
    return buf;
}

// New space is zeroed, as the calloc'd first block is: the lexer's
// strings rely on the zero after the last character written.
void buffer_extend(struct buffer* buffer, size_t size){
    buffer->data = realloc(buffer->data, buffer->msize + size);
    memset(buffer->data + buffer->msize, 0, size);
    buffer->msize += size;
}

// Make room for size more bytes past len, at least doubling.
void buffer_need(struct buffer* buffer, size_t size){
    if((size_t)buffer->msize <= buffer->len + size){
        size_t grow = buffer->len + size + 1 - buffer->msize;
        if(grow < (size_t)buffer->msize){
            grow = buffer->msize;
        }
        buffer_extend(buffer, grow);
    }
}

// Format into the buffer at len and return the formatted length. Every
// printf used to extend the buffer by a fixed 2048 bytes whether it
// needed them or not (and cut longer output off), so the asm buffers
// grew by 2 KB per line written.
static int buffer_vprintf(struct buffer* buffer, const char* fmt, va_list args){
    va_list measure;
    va_copy(measure, args);
    int len = vsnprintf(NULL, 0, fmt, measure);
    va_end(measure);
    if(len < 0){
        return 0;
    }
    buffer_need(buffer, len + 1);
    vsnprintf(&buffer->data[buffer->len], len + 1, fmt, args);
    return len;
}

void buffer_printf(struct buffer* buffer, const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    int actual_len = buffer_vprintf(buffer, fmt, args);
    buffer->len += actual_len;
    va_end(args);
}

void buffer_printf_no_terminator(struct buffer* buffer, const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    int actual_len = buffer_vprintf(buffer, fmt, args);
    buffer->len += actual_len - 1;
    va_end(args);
}

void buffer_write(struct buffer* buffer, char c){
    buffer_need(buffer, sizeof(char));

    buffer->data[buffer->len] = c;
    buffer->len++;
}

void* buffer_ptr(struct buffer* buffer){
    return buffer->data;
}

char buffer_read(struct buffer* buffer){
    if(buffer->rindex >= buffer->len){
        return -1;
    }
    char c = buffer->data[buffer->rindex];
    buffer->rindex++;
    return c;
}

char buffer_peek(struct buffer* buffer){
    if(buffer->rindex >= buffer->len){
        return -1;
    }
    char c = buffer->data[buffer->rindex];
    return c;
}

void buffer_free(struct buffer* buffer){
    free(buffer->data);
    free(buffer);
}
