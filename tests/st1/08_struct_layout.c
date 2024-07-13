// EXPECT: 24 24 16 16 264 520 16 24 256 260
// structs laid out as gcc does (pointers 8-aligned, arrays and
// nested structs counted whole), so structs shared with gcc-built code
// (SamOS's file_dirent and command_argument) agree.
int printf(const char* fmt, ...);
struct e1 { char* a; char* b; int* c; };
struct e2 { char* a; char* b; int (*c)(int x); };
struct e3 { char* a; int* c; };
struct e4 { char c; char* p; };
struct e5 { char name[256]; unsigned int size; unsigned int flags; };
struct e6 { char argument[512]; struct e6* next; };
struct e7 { int argc; char** argv; };
struct e8 { struct e4 x; char y; };
int main(){
    long a = sizeof(struct e1); long b = sizeof(struct e2); long c = sizeof(struct e3); long d = sizeof(struct e4);
    printf("%d %d %d %d ", a, b, c, d);
    a = sizeof(struct e5); b = sizeof(struct e6); c = sizeof(struct e7); d = sizeof(struct e8);
    printf("%d %d %d %d ", a, b, c, d);
    struct e5 v; v.size = 256; v.flags = 260;
    unsigned int* q = (unsigned int*)((char*)&v + 256);
    printf("%d %d\n", *q, *(q + 1));
    return 0;
}
