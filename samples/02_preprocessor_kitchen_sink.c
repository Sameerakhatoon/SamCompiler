// EXPECTED EXIT: 42
//
// Exercises: #define value macros, #define function macros,
// #ifdef / #ifndef / #endif, ##-concat, #x-stringify (via the
// stringify macro), nested expansion. Our preprocessor handles
// #ifdef / #ifndef / #if / #endif but does NOT implement #else,
// so this sample uses paired #ifdef / #ifndef blocks instead of
// an if/else.

#define ANSWER 42
#define DOUBLE(x) ((x) + (x))
#define CAT(a, b) a ## b

#ifdef ANSWER
#define USE_ANSWER 1
#endif

int main()
{
    int CATvar = 0;                // noise to prove the concat is targeted
    int CAT(my, var) = ANSWER;     // myvar = 42 via ## concat
    int doubled       = DOUBLE(21);// ((21)+(21)) = 42

#ifdef USE_ANSWER
    return myvar;
#endif
#ifndef USE_ANSWER
    return doubled;
#endif
}
