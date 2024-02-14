struct s { int x; char c; long l; };
int main(void){ struct s v; struct s* p = &v; char buf[10]; int m[2][3]; char c = 1;
  return sizeof v + sizeof(p) + sizeof *p + sizeof(p->c) + sizeof buf + sizeof m + sizeof m[1] + sizeof(c + 1) + sizeof "abc" + sizeof(1L) / 8; }
