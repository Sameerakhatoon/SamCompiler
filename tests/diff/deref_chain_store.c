/* Stores through chains of dereferences on i386: `(*pp)[i] =
 * c`, `*(*pp) = v`, `(**ppp)[0] = v`, and thread-local pointers, which
 * are such chains there. The `*` before a subscript or another `*` did
 * not load the pointer it read, so the store landed next to it. */
char buf[] = "0123456789";
char* bp = buf;
int iv = 1;
int* ivp = &iv;
_Thread_local char** tpp = &bp;
_Thread_local char* name = buf;
_Thread_local int* tip = &iv;
struct s { int k; int* q; } sv = { 1, 0 };
int main(void){
    char** lpp = &bp;
    int** ipp = &ivp;
    int*** ippp = &ipp;
    struct s* sp = &sv;
    struct s** spp = &sp;
    int r = 0;
    (*lpp)[0] = 'x';
    (*tpp)[6] = 'y';
    name[7] = 'z';
    *(*ipp) = 5;
    if(iv != 5) r |= 8;
    *(*(*ippp)) = 6;
    if(iv != 6) r |= 16;
    (**ippp)[0] = 7;
    if(iv != 7) r |= 32;
    *tip = 8;
    if(iv != 8) r |= 64;
    (*spp)->k = 9;
    sv.q = &iv;
    *(*spp)->q = 10;
    if(sv.k != 9 || iv != 10) r |= 128;
    (*lpp)[1]++;
    (*lpp)[2] += 2;
    *(*lpp + 3) = 'w';
    if(buf[1] != '2' || buf[2] != '4' || buf[3] != 'w') r |= 256;
    return (buf[0] != 'x') + (buf[6] != 'y') * 2 + (buf[7] != 'z') * 4 + (r ? 100 + (r >> 3) : 0);
}
