// short arithmetic wraps at 16 bits.
int main(void){ short s = 30000; s = s + 30000; return s & 127; }
