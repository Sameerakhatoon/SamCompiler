// EXPECT: A "a b" 'c d' it's|x\y|
// \" and the other simple escapes, and quotes inside strings
// (x86_64 writes ' as a number for NASM).
int printf(const char* fmt, ...);
int main(){ printf("A \"a b\" 'c d' it's|x\\y|\n"); return 0; }
