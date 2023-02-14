// EXPECTED EXIT: 99
//
// Exercises: typedef int -> alias, pointer indirection (*),
// address-of (&), assignment through a pointer. Mirrors a
// classic "swap via pointer" pattern except simplified to a
// single-variable update so we don't depend on function arg
// passing of pointers (the ABI for that path is sometimes
// tetchy).

typedef int   my_int;
typedef int*  my_int_ptr;

int main()
{
    my_int     value = 0;
    my_int_ptr p     = &value;

    *p = 99;     // writes 99 through the pointer
    return value;
}
