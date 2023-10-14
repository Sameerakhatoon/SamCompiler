int main(void){ int m[2][3]; int i; int j; for(i = 0; i < 2; i++) for(j = 0; j < 3; j++) m[i][j] = i * 3 + j; return m[1][2] * 10 + m[0][1] + m[i - 1][j - 1]; }
