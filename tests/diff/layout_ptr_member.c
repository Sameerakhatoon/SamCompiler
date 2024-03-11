struct node { int v; struct node* next; };
int main(void){
    struct node b; b.v = 5; b.next = 0;
    struct node a; a.v = 3; a.next = &b;
    struct node* p = &a;
    return p->next->v * 10 + p->v + sizeof(struct node);
}
