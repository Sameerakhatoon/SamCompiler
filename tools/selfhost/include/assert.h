#ifndef SELFHOST_ASSERT_H
#define SELFHOST_ASSERT_H
// The checks stay: some in the sources have side effects
// (`assert(nextc() == start_delim)` in lexer.c), which leaving them out,
// as with NDEBUG, broke. glibc's __assert_fail reports file and line.
void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function);
#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
#define assert(expression) do { if(!(expression)) __assert_fail("assertion", __FILE__, __LINE__, "?"); } while(0)
#endif
#endif
