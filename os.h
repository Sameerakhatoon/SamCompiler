#ifndef SAMCOMPILER_OS_H
#define SAMCOMPILER_OS_H

// The operating-system services the driver needs, in one place
//. os.c has the POSIX implementation and, under
// _WIN32, a Windows one for MinGW-w64: temporary files and directories,
// the driver's own path, running a tool, compiling an input in a
// separate process, and a FILE* that reads a string.

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <stdint.h>
// MinGW has no <elf.h>. The driver reads ELF objects for one fix-up
// (main_mark_tls_externs); these are the parts it uses.
#define EI_CLASS    4
#define ELFCLASS64  2
#define SHT_SYMTAB  2
#define SHN_UNDEF   0
#define STT_TLS     6
#define ELF32_ST_BIND(info)          ((info) >> 4)
#define ELF32_ST_INFO(bind, type)    (((bind) << 4) + ((type) & 0xf))
#define ELF64_ST_BIND(info)          ((info) >> 4)
#define ELF64_ST_INFO(bind, type)    (((bind) << 4) + ((type) & 0xf))
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version, e_entry, e_phoff, e_shoff, e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf32_Ehdr;
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;
typedef struct {
    uint32_t sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size;
    uint32_t sh_link, sh_info, sh_addralign, sh_entsize;
} Elf32_Shdr;
typedef struct {
    uint32_t sh_name, sh_type;
    uint64_t sh_flags, sh_addr, sh_offset, sh_size;
    uint32_t sh_link, sh_info;
    uint64_t sh_addralign, sh_entsize;
} Elf64_Shdr;
typedef struct {
    uint32_t st_name, st_value, st_size;
    unsigned char st_info, st_other;
    uint16_t st_shndx;
} Elf32_Sym;
typedef struct {
    uint32_t st_name;
    unsigned char st_info, st_other;
    uint16_t st_shndx;
    uint64_t st_value, st_size;
} Elf64_Sym;
#else
#include <elf.h>
#endif

// os_run_forked's answer when this system (or SAMCC_NO_FORK) has no fork.
#define OS_NO_FORK (-2)

// Where the driver's private temporary directory goes: $TMPDIR or /tmp;
// on Windows %TMP%, %TEMP% or the current directory.
const char* os_temp_dir(void);

// Replace the trailing XXXXXX of `path_template` and create that file,
// empty, with the usual permissions (0666 & ~umask), then close it.
// Returns 0, or -1 with errno set.
int os_make_temp_file(char* path_template);

// The same for a directory, private to the user (mkdtemp).
int os_make_temp_dir(char* path_template);

// Rename `from` to `to`, replacing `to` if it exists (rename() does not
// replace a file on Windows). Returns 0, or -1 with errno set.
int os_replace_file(const char* from, const char* to);

// The path of the running executable (/proc/self/exe on Linux,
// GetModuleFileNameA on Windows). False when there is no way to know.
bool os_self_path(char* out, size_t out_size);

// The part of `path` after its last directory separator ('/', and '\\'
// on Windows too).
const char* os_base_name(const char* path);

// Quote `in` as one argument of a command line for os_run_command:
// single quotes for /bin/sh, the MSVCRT rules (double quotes) on
// Windows. False when `out` is too small.
bool os_quote_arg(const char* in, char* out, size_t out_size);

// Run a command line (system() on POSIX, CreateProcessA on Windows, no
// cmd.exe). Returns 0, or the tool's exit status (at least 1); 127 when
// it could not be started.
int os_run_command(const char* cmd);

// Whether `tool` can be found on the PATH (`command -v`).
bool os_have_command(const char* tool);

// Call fn(arg) in a forked child and wait for it: 0 when it returned 0,
// 1 when it failed or exited some other way. OS_NO_FORK on Windows, and
// on POSIX when the environment variable SAMCC_NO_FORK is set, so the
// caller's other way can be tested on Linux too.
int os_run_forked(int (*fn)(void* arg), void* arg);

// A FILE* that reads `text`; fclose releases it (fmemopen on POSIX, a
// temporary file deleted on close on Windows).
FILE* os_memory_file(const char* text);

#endif
