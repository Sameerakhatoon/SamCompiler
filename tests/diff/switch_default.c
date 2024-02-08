int f(int x){ switch(x){ case 0: return 1; default: return 2; case 5: return 3; } }
int main(void){ return f(0) + f(5) * 10 + f(9) * 100 % 97; }
