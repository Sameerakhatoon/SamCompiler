/* Smaller things SamCompiler's own sources use. */
typedef void (*GEN_END)(int x);
#define GEN_END(x) ((x) + 1)

struct dt { int flags; int depth; };
struct vt { int k; struct dt t; struct vt* next; };

static int seen;
static int seen;
static int count;
static int count = 3;

static void end(int x){ seen = x; }

struct holder { long a; long b; };
struct sw { _Bool on; int flags; };
static struct sw* sw_of(struct sw* s){ return s; }
void* calloc(unsigned long n, unsigned long size);

int main(void){
    GEN_END f = end;
    struct vt v = { 0 };
    struct vt* p = &v;
    struct holder* h = calloc(1, sizeof(*h));
    int n = sizeof n;
    f(GEN_END(4));
    if(seen != 5) return 1;
    v.t.flags++;
    p->t.depth++;
    p->t.depth++;
    int old = v.t.flags++;
    if(old != 1 || v.t.flags != 2 || v.t.depth != 2) return 2;
    if(sizeof(*h) != 2 * sizeof(long) || n != sizeof(int)) return 3;
    if(count != 3 || seen != 5) return 4;
    struct sw w = { 0 };
    sw_of(&w)->on = 0x100;
    sw_of(&w)->flags |= 6;
    if(w.on != 1 || w.flags != 6) return 5;
    return 0;
}
