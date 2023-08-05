struct node { int v; struct node* next; };
int main(void){ struct node a; struct node b; a.v = 1; b.v = 2; a.next = &b; b.next = 0; return a.next->v * 10 + a.v; }
