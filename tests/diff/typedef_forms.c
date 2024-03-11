typedef struct { int x; int y; } Point;
typedef struct Node { int v; struct Node* next; } Node;
typedef union { int i; char c; } IntOrChar;
typedef struct Node* NodePtr;
typedef struct { int a; } *APtr;
typedef unsigned int u32;
int main(void){
    Point p; p.x = 2; p.y = 3;
    Node n2; n2.v = 5; n2.next = 0;
    Node n1; n1.v = 4; n1.next = &n2;
    NodePtr q = &n1;
    IntOrChar ic; ic.i = 0; ic.c = 9;
    u32 big = 4000000000u;
    struct { int a; } s; s.a = 1;
    return p.x * p.y + q->next->v + ic.i + (big > 0) + sizeof(Point) + sizeof(IntOrChar);
}
