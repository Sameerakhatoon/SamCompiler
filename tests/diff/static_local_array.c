int f(int i){ static int seen[4]; seen[i]++; return seen[i]; } int main(void){ f(1); f(1); return f(1) * 10 + f(2); }
