struct n { int v; struct n* next; };
int main(void){ struct n a; struct n b; struct n c; a.v = 1; b.v = 2; c.v = 3; a.next = &b; b.next = &c; c.next = 0;
  int s = 0; struct n* p = &a; while(p){ s = s * 10 + p->v; p = p->next; } return s % 100; }
