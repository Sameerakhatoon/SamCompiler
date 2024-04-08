struct in { int a; int b; }; struct out { int k; struct in i; int t[2]; }; int main(void){ struct out o = {1, {2, 3}, {4, 5}}; return o.k + o.i.a + o.i.b + o.t[0] + o.t[1]; }
