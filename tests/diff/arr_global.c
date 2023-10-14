int g[3];
int main(void){ int i; for(i = 0; i < 3; i++) g[i] = i + 1; int* p = g; return g[0] + g[1] * 10 + p[2]; }
