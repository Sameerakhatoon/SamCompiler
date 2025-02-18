// built by gcc; compares SamCompiler's table (table.c) with
// gcc's, byte for byte in the bytes that hold the value (10 in the x87
// format, 16 in binary128), and prints a line per difference.
#include <stdio.h>
#include <string.h>

extern long double table[], gcc_table[];
extern int table_count, gcc_table_count;

int main(void){
    int bytes = sizeof(long double) == 16 && __LDBL_MANT_DIG__ == 113 ? 16 : 10;
    int bad = 0;
    if(table_count != gcc_table_count){
        printf("count %d, gcc %d\n", table_count, gcc_table_count);
        return 1;
    }
    for(int i = 0; i < table_count; i++){
        if(memcmp(&table[i], &gcc_table[i], bytes) == 0){
            continue;
        }
        unsigned char* s = (unsigned char*)&table[i];
        unsigned char* g = (unsigned char*)&gcc_table[i];
        printf("table[%d]: sam ", i);
        for(int k = bytes - 1; k >= 0; k--) printf("%02x", s[k]);
        printf(", gcc ");
        for(int k = bytes - 1; k >= 0; k--) printf("%02x", g[k]);
        printf("\n");
        bad++;
    }
    if(!bad){
        printf("ok\n");
    }
    return bad != 0;
}
