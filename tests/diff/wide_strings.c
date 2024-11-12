// Wide string and character literals: L"" arrays of wchar_t, u"" of
// char16_t and U"" of char32_t, with escapes, UTF-8 source characters,
// concatenation, sizeof, array initializers, file-scope pointers and
// a literal returned from a function (static storage).
#include <stddef.h>

const wchar_t* greeting = L"hi";
wchar_t global_text[] = L"abc";
const wchar_t* names[] = { L"one", L"three" };

const wchar_t* pick(int i){
    return i ? L"yes" : L"no";
}

int wide_length(const wchar_t* s){
    int n = 0;
    while(s[n]){
        n++;
    }
    return n;
}

int main(void){
    wchar_t local[] = L"wide";
    wchar_t sized[8] = L"ab";
    const wchar_t* p = L"x\x41\101é\n";
    const unsigned short* u = u"€z";
    const unsigned int* big = U"\U0001F600";
    const unsigned short* pair = u"\U0001F600";
    const wchar_t* utf8 = L"é€";
    const wchar_t* joined = L"ab" "cd";
    int sum = 0;
    if(sizeof(L"abc") != 4 * sizeof(wchar_t)) return 1;
    if(sizeof(local) != 5 * sizeof(wchar_t)) return 2;
    if(sizeof(u"ab") != 6) return 3;
    if(wide_length(local) != 4 || local[3] != L'e') return 4;
    if(sized[1] != 'b' || sized[2] != 0 || sized[7] != 0) return 5;
    if(p[1] != 'A' || p[2] != 'A' || p[3] != 0xe9 || p[4] != 10 || p[5] != 0) return 6;
    if(u[0] != 0x20ac || u[1] != 'z') return 7;
    if(big[0] != 0x1F600 || big[1] != 0) return 8;
    if(pair[0] != 0xD83D || pair[1] != 0xDE00) return 9;
    if(utf8[0] != 0xe9 || utf8[1] != 0x20ac || utf8[2] != 0) return 10;
    if(wide_length(joined) != 4 || joined[2] != 'c') return 11;
    if(L'a' != 97 || L'\x263a' != 0x263a || u'é' != 233 || U'z' != 122) return 12;
    if(sizeof(L'a') != 4) return 13;
    if(wide_length(greeting) != 2 || global_text[2] != 'c' || sizeof(global_text) != 16) return 14;
    if(names[1][4] != 'e' || wide_length(names[0]) != 3) return 15;
    const wchar_t* yes = pick(1);
    const wchar_t* no = pick(0);
    if(yes[0] != 'y' || no[1] != 'o' || wide_length(yes) != 3) return 16;
    for(const wchar_t* q = L"\x10\x20"; *q; q++){
        sum = sum + *q;
    }
    return sum;
}
