/* Struct tags are a name space of their own (C11 6.2.3), and an
   untagged struct member's type takes no name from the member. */
struct config { int fix; int end; };
static int config(struct config* c){ return c->fix + c->end; }

struct scope { int depth; };
struct process {
    struct { struct scope* root; struct scope* current; } scope;
    int config;
};

union value { int i; char c; };
static int value(void){ return 7; }

int main(void){
    struct config c = { 2, 3 };
    struct scope s = { 4 };
    struct process p;
    union value v;
    p.scope.root = &s;
    p.scope.current = &s;
    p.config = config(&c);
    v.i = value();
    if(p.config != 5) return 1;
    if(p.scope.root->depth + p.scope.current->depth != 8) return 2;
    if(v.i != 7) return 3;
    return 0;
}
