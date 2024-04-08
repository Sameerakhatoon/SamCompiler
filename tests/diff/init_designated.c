struct p { int x; int y; int z; }; int main(void){ struct p v = {.z = 3, .x = 1}; int a[5] = {[3] = 7, [1] = 2}; return v.x + v.y * 10 + v.z * 3 + a[3] + a[1] + a[0]; }
