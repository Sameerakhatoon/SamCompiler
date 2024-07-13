// EXPECT: 1 ok
// pointers to a struct that is declared and never defined, as
// SamOS's samos.h declares `struct file_stat;` for programs that do
// not include file.h.
int printf(const char* fmt, ...);
struct opaque;
int keep(struct opaque* p);
struct opaque* global_p;
int keep(struct opaque* p){ return p == 0; }
int main(){ struct opaque* q = 0; global_p = q; printf("%d %s\n", keep(q), "ok"); return 0; }
