// EXPECTED EXIT: 120
int g = 20;
char k = 100;
const char* msg = "x";

int main()
{
    return g + k + msg[1];
}
