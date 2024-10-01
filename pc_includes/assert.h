// No include guard: as in C, each inclusion redefines assert for the
// current NDEBUG. A failed assertion calls
// glibc's __assert_fail, which prints the message and aborts.
#undef assert
#ifdef NDEBUG
#define assert(expression) ((void)0)
#endif
#ifndef NDEBUG
void __assert_fail(const char* expression, const char* file, unsigned int line, const char* function);
#define assert(expression) ((expression) ? (void)0 : __assert_fail(#expression, __FILE__, __LINE__, "?"))
#endif
