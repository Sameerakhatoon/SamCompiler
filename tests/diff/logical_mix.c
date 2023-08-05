int main(void){ int a = 3; int b = 0; int r = (a && !b) + (a || b) * 2 + (!a || b) * 4; return r; }
