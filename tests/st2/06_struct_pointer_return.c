// EXPECT: ab 7 cd
// a function returning a pointer to a struct takes no hidden
// result argument; its parameters were read one slot too far up.
int printf(const char* fmt, ...);
struct node { char name[8]; struct node* next; };
struct node nodes[2];
struct node* pick(const char* name, int max){
    if(max > 100){
        return 0;
    }
    struct node* n = &nodes[max & 1];
    n->name[0] = name[0];
    n->name[1] = name[1];
    return n;
}
int main(){
    struct node* a = pick("ab", 7);
    struct node* b = pick("cd", 6);
    printf("%s %d %s\n", a->name, (int)(a - nodes) + 6, b->name);
    return 0;
}
