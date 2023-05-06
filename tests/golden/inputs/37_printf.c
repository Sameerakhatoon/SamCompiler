// EXPECTED EXIT: 0
// EXPECTED OUTPUT: v=42 s=hi
int printf(const char* fmt, ...);

int main()
{
    printf("v=%d s=%s\n", 42, "hi");
    return 0;
}
