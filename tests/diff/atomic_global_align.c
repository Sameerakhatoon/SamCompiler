/* An _Atomic global is aligned to its size, as gcc does, also after a
 * char: on x86_64 scalar globals were packed and `_Atomic double d`
 * after a `char` sat at an odd address.
 * The exit status is a bit per misaligned object. */
char c1 = 1;
_Atomic double d = 2.5;
char c2 = 3;
_Atomic long l = 4;
char c3;
_Atomic int i;
char c4;
_Atomic short s;
char c5 = 1;
_Atomic long long ll = 5;
char c6 = 1;
_Atomic float f = 1.5f;
char c7;
static _Atomic double sd;
char c8 = 2;
_Atomic(char*) p;

int main(void){
    int bad = 0;
    if((unsigned long)&d % sizeof(double)) bad |= 1;
    if((unsigned long)&l % sizeof(long)) bad |= 2;
    if((unsigned long)&i % 4) bad |= 4;
    if((unsigned long)&s % 2) bad |= 8;
    if((unsigned long)&f % 4) bad |= 16;
    if(sizeof(long) == 8 && (unsigned long)&ll % 8) bad |= 32;
    if((unsigned long)&sd % sizeof(double)) bad |= 64;
    if((unsigned long)&p % sizeof(char*)) bad |= 128;
    sd = d;
    return bad + (sd == 2.5 ? 0 : 3) + c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8 - 8;
}
