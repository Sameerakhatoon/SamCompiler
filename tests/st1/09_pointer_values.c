// EXPECT: 7 t1 1 b 42 42 s1 s1 1 2 one two 2 one
// values reached through chains: an array read with a variable
// index, an element of an array of pointers, a pointer member of a
// variable-indexed struct array, p->next->x, an array member used as
// a pointer, `!*p`.
int printf(const char* fmt, ...);
char* strcpy(char* d, const char* s);
char* tab[3];
struct n { int x; char* s; struct n* next; };
struct n nodes[3];
struct n na; struct n nb;
struct ca { char argument[16]; struct ca* next; };
struct ca g1; struct ca g2;
int arr[4];
int main(){
    long i = 1; arr[1] = 7; tab[1] = "t1";
    na.next = &nb; nb.x = 42; nb.s = "s1"; nb.next = 0; nodes[1].s = "s1"; nodes[1].next = 0;
    struct n* p = &na;
    char* v[3]; v[1] = "ab";
    printf("%d %s %c %c ", arr[i], tab[i], tab[i][1], v[i][1]);
    printf("%d %d %s %s %d ", p->next->x, (p->next)->x, p->next->s, nodes[i].s, nodes[i].next == 0);
    int c = 0; for(struct n* r = p; r; r = r->next){ c = c + 1; }
    strcpy(g1.argument, "one"); strcpy(g2.argument, "two"); g1.next = &g2; g2.next = 0;
    char* w[4]; int k = 0;
    for(struct ca* a = &g1; a && k < 4; a = a->next){ w[k++] = a->argument; }
    const char* e = "";
    printf("%d %s %s %d %s\n", c, w[0], w[1], k + !*e - 1, g1.argument);
    return 0;
}
