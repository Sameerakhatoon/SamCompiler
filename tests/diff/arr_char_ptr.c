int len(char* s){ int n = 0; while(*s){ n++; s++; } return n; }
int main(void){ char buf[8]; buf[0] = 'a'; buf[1] = 'b'; buf[2] = 'c'; buf[3] = 0; return len(buf) + len("hello") * 10; }
