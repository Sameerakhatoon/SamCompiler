int main(void){ unsigned int u = 5; unsigned int n = -u; unsigned int t = ~u; return (n == 4294967291u) + (t == 4294967290u) * 2 + (-u > 0) * 4; }
