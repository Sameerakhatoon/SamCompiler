// char arithmetic wraps at 8 bits before the next use.
int main(void){ char c = 100; c = c + 100; return c + 100; }
