// EXPECTED EXIT: 60
//
// Exercises: struct definition with multiple members + a
// self-referential next-index slot, array of structs, pointer
// member access (.field), traversal pattern that mimics a
// linked list using array indices instead of malloc.
//
// nodes[] holds five nodes; node 0 is the head; each node's
// `next` field is the index of the next node, or -1 to terminate.
// We walk head -> next -> ... summing values:
//   10 + 20 + 30 = 60.

struct node {
    int value;
    int next;
};

struct node nodes[5];

int sum_from(int start)
{
    int total = 0;
    int i = start;
    while (i >= 0) {
        total = total + nodes[i].value;
        i = nodes[i].next;
    }
    return total;
}

int main()
{
    // Build the chain: nodes[0] -> nodes[2] -> nodes[4] -> end
    nodes[0].value = 10;
    nodes[0].next  = 2;
    nodes[2].value = 20;
    nodes[2].next  = 4;
    nodes[4].value = 30;
    nodes[4].next  = -1;

    // Distractors that should be skipped by the walk.
    nodes[1].value = 999;
    nodes[1].next  = -1;
    nodes[3].value = 777;
    nodes[3].next  = -1;

    return sum_from(0);
}
