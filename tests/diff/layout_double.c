struct d { char c; double v; };
struct e { int i; char k; };
int main(void){ return sizeof(struct d) + sizeof(struct e) * 2; }
