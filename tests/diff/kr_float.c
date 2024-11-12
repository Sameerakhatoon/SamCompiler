// Old-style (K&R) definitions with float and double parameters: the
// caller promotes a float argument to double, and the definition's
// float parameter is converted back from it.
double kr(x, y, n) float x; double y; int n; { return x * 4 + y + n; }
double krd(a, b) double a, b; { return a - b; }

int main(void){
    if(kr(1.5, 2.0, 3) != 11.0) return 1;
    if(krd(5.5, 2.25) != 3.25) return 2;
    return 0;
}
