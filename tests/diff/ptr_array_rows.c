int main(void){ int m[3][4]; int i; int j; for(i = 0; i < 3; i++) for(j = 0; j < 4; j++) m[i][j] = i + j; int* row = m[2]; return row[3] * 10 + m[1][2]; }
