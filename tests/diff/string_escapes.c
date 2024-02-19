int main(void){ const char* s = "it's\a\b\f\v\r\?\\\"\x7f\101"; int n = 0; int i = 0; while(s[i]){ n = n + s[i]; i++; } return n % 113; }
