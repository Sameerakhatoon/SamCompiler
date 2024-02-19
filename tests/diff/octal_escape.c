int main(void){ char c = '\101'; char d = '\x42'; const char* s = "\x41\102"; return c + d - 120 + s[1] - 60; }
