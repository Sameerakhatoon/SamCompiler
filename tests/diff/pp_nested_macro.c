#define B 2
#define F(x) ((x) + B)
#define G(x) F(x) * 3
int main(void){ return G(F(1)) + F(B); }
