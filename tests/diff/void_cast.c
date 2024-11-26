// `(void)` casts as values and statements, as assert's expansion uses
// them (`cond ? (void)0 : f()`). On x86 the cast to void wrote
// `movsx eax, (null)`.
int calls;
void note(int x){ calls += x; }
int main(void){
    int a = 3;
    (void)a;
    (void)note(2);
    a > 1 ? (void)0 : note(100);
    a < 1 ? (void)0 : note(5);
    (void)(a + 1);
    return calls + a;
}
