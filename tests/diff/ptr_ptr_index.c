// A subscript of a pointer that is itself an element or a member:
// `a[i][j]` with `char** a`, `v[i][j]` with `int* v[2]`, `sp->name[i]`.
// On x86 the pointer was not loaded, so the bytes of the pointer were
// read.
struct named { int* p; char* name; char** list; };

int first_letter(char** words, int i){
    return words[i][0];
}

int main(void){
    char* words[2];
    char second[4] = "xyz";
    words[0] = "ab";
    words[1] = second;
    char** w = words;
    w[1][1] = 'Q';
    int a = 1, b = 7;
    int* v[2];
    v[0] = &a;
    v[1] = &b;
    v[1][0] = 8;
    struct named n;
    n.p = &a;
    n.name = "qr";
    n.list = words;
    struct named* sp = &n;
    int i = 1;
    sp->p[0] = 3;
    sp->list[0] = "cd";
    int total = first_letter(words, 1) + w[0][i] + v[1][0] * 2 + v[0][0]
        + sp->name[i] + sp->name[0] + sp->list[1][i] + second[1] + words[0][0];
    return total % 127;
}
