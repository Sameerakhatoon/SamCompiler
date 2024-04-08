int f(void){ goto out; return 1; out: return 2; } int main(void){ int r = 0; goto out; r = 9; out: return r + f(); }
