// A member declared with an anonymous struct or union type (SamOS
// ktest's window event): it was laid out as a stack variable and
// overlapped the members before it.
struct ev { int type; int win_id; void* window; struct { int x; int y; } data; };
struct mixed { char tag; union { int i; char c[8]; } u; struct { short a; long b; } s[2]; int last; };
static int calls;
int get_event(struct ev* e){
    if(calls >= 2) return -1;
    e->type = 1; e->win_id = 3; e->window = 0;
    e->data.x = calls == 0 ? 'x' : 'q'; e->data.y = 7; calls++;
    return 0;
}
int main(void){
    char got[64]; int n = 0; struct ev e;
    while (n < (int)sizeof(got) - 1 && get_event(&e) == 0) { if (e.type == 1) got[n++] = (char)e.data.x; }
    got[n] = 0;
    struct mixed m; m.tag = 1; m.u.i = 2; m.s[1].a = 3; m.s[1].b = 4; m.last = 5;
    return (got[0] == 'x' && got[1] == 'q' && got[2] == 0) + (int)sizeof(struct ev) + (int)sizeof(struct mixed)
        + m.tag + m.u.i + m.s[1].a + (int)m.s[1].b + m.last + e.data.y;
}
