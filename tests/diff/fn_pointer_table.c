int one(void){ return 1; } int two(void){ return 2; } int main(void){ int (*t[2])(void) = {one, two}; return t[0]() + t[1]() * 10; }
