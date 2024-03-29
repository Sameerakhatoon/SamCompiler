enum color { RED, GREEN = 5, BLUE, LAST = BLUE * 2 + 1 };
enum color g = BLUE;
int main(void){
    enum color c = GREEN;
    int r = RED + c + g + LAST;
    return r + sizeof(enum color);
}
