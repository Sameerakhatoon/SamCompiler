// The vprintf family for a self-compiled SamCompiler.
//
// SamCompiler's va_list on the 64-bit targets is its own layout (five
// addresses), not the psABI's, so a va_list it made cannot be
// handed to the C library's vfprintf. tools/selfhost/include/stdio.h
// maps vfprintf, vsnprintf, vsprintf and vprintf to the functions here.
// They are compiled by SamCompiler too: each conversion's argument is
// read with va_arg and printed by the C library's snprintf, which takes
// a plain argument list.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Append `len` bytes of `text` to `out` (of `size` bytes) at *total,
// as far as they fit before the terminating 0.
static void sam_put(char* out, size_t size, size_t* total, const char* text, size_t len){
    for(size_t i = 0; i < len; i++){
        if(out && *total + 1 < size){
            out[*total] = text[i];
        }
        *total = *total + 1;
    }
}

// Append one conversion's text, which sam_format_* allocated, and free it.
static void sam_put_spec(char* out, size_t size, size_t* total, char* piece, int len){
    sam_put(out, size, total, piece, len);
    free(piece);
}

static char* sam_format_long(const char* spec, long value, int* len){
    *len = snprintf(NULL, 0, spec, value);
    char* piece = malloc(*len + 1);
    snprintf(piece, *len + 1, spec, value);
    return piece;
}

static char* sam_format_int(const char* spec, int value, int* len){
    *len = snprintf(NULL, 0, spec, value);
    char* piece = malloc(*len + 1);
    snprintf(piece, *len + 1, spec, value);
    return piece;
}

static char* sam_format_double(const char* spec, double value, int* len){
    *len = snprintf(NULL, 0, spec, value);
    char* piece = malloc(*len + 1);
    snprintf(piece, *len + 1, spec, value);
    return piece;
}

static char* sam_format_pointer(const char* spec, void* value, int* len){
    *len = snprintf(NULL, 0, spec, value);
    char* piece = malloc(*len + 1);
    snprintf(piece, *len + 1, spec, value);
    return piece;
}

// Append the decimal digits of n to spec at *at (a `*` width or
// precision taken from the arguments).
static void sam_spec_number(char* spec, int* at, int n){
    char digits[16];
    int count = snprintf(digits, sizeof(digits), "%d", n);
    for(int i = 0; i < count && *at < 60; i++){
        spec[*at] = digits[i];
        *at = *at + 1;
    }
}

int sam_vsnprintf(char* out, size_t size, const char* fmt, va_list args){
    size_t total = 0;
    const char* p = fmt;
    while(*p){
        if(*p != '%'){
            sam_put(out, size, &total, p, 1);
            p++;
            continue;
        }
        if(p[1] == '%'){
            sam_put(out, size, &total, "%", 1);
            p += 2;
            continue;
        }
        char spec[64];
        int at = 0;
        spec[at++] = *p++;
        while(*p && strchr("-+ #0", *p) && at < 60){
            spec[at++] = *p++;
        }
        if(*p == '*'){
            sam_spec_number(spec, &at, va_arg(args, int));
            p++;
        }
        while(*p >= '0' && *p <= '9' && at < 60){
            spec[at++] = *p++;
        }
        if(*p == '.'){
            spec[at++] = *p++;
            if(*p == '*'){
                sam_spec_number(spec, &at, va_arg(args, int));
                p++;
            }
            while(*p >= '0' && *p <= '9' && at < 60){
                spec[at++] = *p++;
            }
        }
        // The length: every integer wider than int is read as a long
        // and printed with `l` (long and long long are both 64 bits).
        int is_long = 0;
        while(*p && strchr("hlLqjzt", *p)){
            if(*p != 'h'){
                is_long = 1;
            }
            p++;
        }
        char conversion = *p;
        if(!conversion){
            break;
        }
        p++;
        if(is_long && strchr("diouxX", conversion)){
            spec[at++] = 'l';
        }
        spec[at++] = conversion;
        spec[at] = 0;
        int len = 0;
        char* piece = 0;
        if(strchr("diouxX", conversion)){
            piece = is_long ? sam_format_long(spec, va_arg(args, long), &len)
                            : sam_format_int(spec, va_arg(args, int), &len);
        } else if(conversion == 'c'){
            piece = sam_format_int(spec, va_arg(args, int), &len);
        } else if(strchr("fFeEgGaA", conversion)){
            piece = sam_format_double(spec, va_arg(args, double), &len);
        } else if(conversion == 's' || conversion == 'p'){
            piece = sam_format_pointer(spec, va_arg(args, void*), &len);
        } else if(conversion == 'n'){
            *va_arg(args, int*) = (int)total;
            continue;
        } else {
            // Not a conversion: print it as it was written.
            sam_put(out, size, &total, spec, at);
            continue;
        }
        sam_put_spec(out, size, &total, piece, len);
    }
    if(out && size){
        out[total < size ? total : size - 1] = 0;
    }
    return (int)total;
}

int sam_vsprintf(char* out, const char* fmt, va_list args){
    return sam_vsnprintf(out, (size_t)-1 / 2, fmt, args);
}

int sam_vfprintf(FILE* stream, const char* fmt, va_list args){
    va_list measure;
    va_copy(measure, args);
    int len = sam_vsnprintf(NULL, 0, fmt, measure);
    va_end(measure);
    char* text = malloc(len + 1);
    sam_vsnprintf(text, len + 1, fmt, args);
    fwrite(text, 1, len, stream);
    free(text);
    return len;
}

int sam_vprintf(const char* fmt, va_list args){
    return sam_vfprintf(stdout, fmt, args);
}
