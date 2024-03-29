int a = 3 + 4;
int b = -3;
long c = 10UL;
unsigned d = -1;
char e = 300;
int f = (1 << 5) | 3;
int g = 7 / 2 + 7 % 2;
int h = sizeof(long) * 2;
int k = 1 ? 9 : 1 / 0;
int arr[2 * 3];
int main(void){
    return a + b + (c == 10) + (d == 4294967295u) + e + f + g + h + k + sizeof(arr) / sizeof(arr[0]);
}
