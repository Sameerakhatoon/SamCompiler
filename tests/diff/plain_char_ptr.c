int main(void){ char s[3]; s[0] = (char)230; s[1] = 1; s[2] = 0; char* p = s; return (*p > 100) + (p[0] < 0) * 2 + 10; }
