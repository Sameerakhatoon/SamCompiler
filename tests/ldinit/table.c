// long double constant expressions in global initializers.
// tests/510-long-double-init.sh builds this file with SamCompiler and,
// renamed to gcc_table, with gcc, and main.c compares the bytes.
long double table[] = {
    1.0L / 3, 2.0L / 3, -1.0L / 3, 1.0L / 7, 22.0L / 7, 1.0L / 10, 1.0L / 49 * 49,
    2.0L * 3, -1.5L + 1, 1.0L - 1.0L, -0.0L - 0.0L, -0.0L + 0.0L, -0.0L * 5, 0.0L / -3,
    0.1L + 0.2L, 0.1L * 3, 0.3L - 0.1L, 1e10L * 1e10L, 1e-10L / 1e10L,
    1 + 1e-19L, 1 + 1e-30L, 1 + 1e-34L, 1 - 1e-34L, 1 - 1e-200L, 1 + 1e-200L, -1 - 1e-200L,
    1e4000L * 1e4000L, -1e4000L * 1e4000L, 1e-4000L * 1e-900L, 1e-4940L / 3, 1e-4930L / 1e10L,
    1e-4950L - 1e-4950L * 2, 1.0L / 0x1p16382L, 0x1p-16382L * 0.5L, 0x1.8p-16445L * 0.5L,
    3.0L * 0.1, 0.1 + 0.2L, 3 * 1.0L / 7, 2147483647 * 3.0L, -2147483647 - 1 + 0.5L,
    4294967295u / 7.0L, (long double)1 / 3 + (long double)2 / 3, 1.5L * 1.5L * 1.5L * 1.5L,
    +(1.0L / 3), -(1.0L / 3) * 3, (1.0L / 3 - 1.0L / 3) * -1, 1.0L / 0, -1.0L / 0, 1e4000L / 1e-4000L,
    123456789.0L * 987654321.0L * 1e20L, 0x1.fffffffffffffffep0L + 0x1p-64L,
    0x1.ffffffffffffffffffffffffffffp0L + 0x1p-113L, 0x1.0000000000000001p0L * 0x1.0000000000000001p0L,
};
int table_count = sizeof(table) / sizeof(table[0]);
