int main(void){
    const char* f = __FILE__;
    int n = 0;
    while (f[n]) n++;
    return (n > 5) + __LINE__;
}
