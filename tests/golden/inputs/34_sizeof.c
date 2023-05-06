// EXPECTED EXIT: 7
struct three {
    char a;
    char b;
    char c;
};

int main()
{
    return sizeof(int) + sizeof(struct three);
}
