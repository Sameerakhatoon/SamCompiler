int main(void){ int s = 0; for(int i = 0; i < 3; i++){ int t = i * 2; for(int j = 0, k = 1; j < 2; j++){ s += t + j + k; } } int i = 100; return s + i % 7; }
