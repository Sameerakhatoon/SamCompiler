int main(void){ char s[6]; s[0] = 'h'; s[1] = 'e'; s[2] = 'l'; s[3] = 'l'; s[4] = 'o'; s[5] = 0; int n = 0; int i = 0; while(s[i]){ n = n + s[i] - 'a'; i++; } return n; }
