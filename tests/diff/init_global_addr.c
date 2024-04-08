int g = 5; int* p = &g; int arr[3]; int* q = arr; char* s = "x"; int main(void){ return *p + (q == arr) + s[0] - 120; }
