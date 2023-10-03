int g = 5;
int counter(void){ return g++; }
int main(void){ counter(); counter(); int v = counter(); g -= 1; return v * 10 + g; }
