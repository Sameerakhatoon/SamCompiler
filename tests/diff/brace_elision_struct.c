/* `= { 0 }` and positional values reaching into a struct first member. */
struct rule { int left; int right; };
struct entity { struct rule rule; int flags; long value; };
struct pair { int a; long b; };
struct nest { struct pair p; struct pair q; int last; };

static struct pair make(int a, long b){ struct pair p = { a, b }; return p; }

struct entity global_entity = { 0 };
struct nest global_nest = { 1, 2, 3, 4, 5 };

int main(void){
    struct entity e = { 0 };
    struct nest n = { 1, 2, { 3, 4 }, 5 };
    struct nest m = { make(6, 7), 8, 9 };
    if(e.rule.left || e.rule.right || e.flags || e.value) return 1;
    if(global_entity.rule.right || global_entity.value) return 2;
    if(global_nest.p.a != 1 || global_nest.q.b != 4 || global_nest.last != 5) return 3;
    if(n.p.b != 2 || n.q.a != 3 || n.last != 5) return 4;
    if(m.p.a != 6 || m.p.b != 7 || m.q.a != 8 || m.q.b != 9) return 5;
    return 0;
}
