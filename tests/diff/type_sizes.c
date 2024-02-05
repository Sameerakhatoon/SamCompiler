int main(void){ return sizeof(char) + sizeof(short) * 2 + sizeof(int) * 4 + sizeof(short int) * 8 + (sizeof(long int) == sizeof(long)) * 50; }
