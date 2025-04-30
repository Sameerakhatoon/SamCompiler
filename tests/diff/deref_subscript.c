/* A subscript of a dereference, `(*pp)[i]`, read and stored, and so on
 * i386 a subscript of a thread-local pointer (`name[1]` with
 * `_Thread_local char* name`, which is `(*addr)[1]` there): a read
 * pushed the element's address in place of its value, and a store went
 * to the bytes after *pp's own address
 *. */
_Thread_local char* name = "abcdefg";
char buf[] = "0123456789";
char* bp = buf;
int ib[] = { 10, 11, 12, 13 };
int* ip = ib;
_Thread_local char** tpp = &bp;
struct s { int k; short h[3]; };
struct s* sp;

static int at(char** pp, int i){
    return (*pp)[i];
}

static void put(int** pp, int i, int v){
    (*pp)[i] = v;
}

int main(void){
    char** lpp = &bp;
    int** ipp = &ip;
    struct s sv = { 1, { 4, 5, 6 } };
    sp = &sv;
    struct s** spp = &sp;
    int r = 0;
    int i = 3;
    if((*lpp)[2] != '2') r |= 1;
    if((*ipp)[2] != 12) r |= 2;
    if((*ipp)[i] + 1 != 14) r |= 4;
    if(name[1] != 'b' || name[i] != 'd') r |= 8;
    if((*tpp)[4] != '4') r |= 16;
    if(at(&bp, 5) != '5') r |= 32;
    char c = (*lpp)[1] + 1;
    if(c != '2') r |= 64;
    (*lpp)[0] = 'x';
    (*ipp)[1] = 21;
    (*ipp)[i] += 5;
    (*tpp)[6] = 'y';
    put(&ip, 0, 30);
    (*spp)->k = 9;
    (*spp)->h[1] = 7;
    if(buf[0] != 'x' || buf[6] != 'y') r |= 128;
    if(ib[0] != 30 || ib[1] != 21 || ib[3] != 18) r |= 256;
    if(sv.k != 9 || sv.h[1] != 7 || (*spp)->h[2] != 6) r |= 512;
    return r ? 1 + (r & 63) + (r >> 6) * 64 % 255 : 0;
}
