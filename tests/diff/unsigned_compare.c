int main(void){ unsigned int a = 4000000000u; int r = 0; if(a > 1) r = r + 1; if(a > 2000000000) r = r + 2; if(-1 < (int)a) r = r + 4; if(a >= 4000000000u) r = r + 8; return r; }
