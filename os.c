#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os.h"

// The operating-system layer (os.h). Everything
// the driver does that POSIX and Windows do differently is here, once
// per system; the rest of the compiler is plain C.

#ifdef _WIN32

#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>

const char* os_temp_dir(void){
    const char* dir = getenv("TMP");
    if(!dir || !dir[0]){
        dir = getenv("TEMP");
    }
    return dir && dir[0] ? dir : ".";
}

int os_make_temp_file(char* path_template){
    // _mktemp_s picks a name no file has yet; _O_EXCL makes sure no one
    // took it in between.
    if(_mktemp_s(path_template, strlen(path_template) + 1) != 0){
        errno = EEXIST;
        return -1;
    }
    int fd;
    if(_sopen_s(&fd, path_template, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0){
        return -1;
    }
    _close(fd);
    return 0;
}

int os_make_temp_dir(char* path_template){
    if(_mktemp_s(path_template, strlen(path_template) + 1) != 0){
        errno = EEXIST;
        return -1;
    }
    return _mkdir(path_template) == 0 ? 0 : -1;
}

int os_replace_file(const char* from, const char* to){
    if(MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING)){
        return 0;
    }
    errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : EIO;
    return -1;
}

bool os_self_path(char* out, size_t out_size){
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)out_size);
    return n > 0 && n < out_size;
}

const char* os_base_name(const char* path){
    const char* base = path;
    for(const char* c = path; *c; c++){
        if(*c == '/' || *c == '\\'){
            base = c + 1;
        }
    }
    return base;
}

// The MSVCRT command-line rules: the argument in double quotes, a run of
// backslashes doubled when a quote (an escaped one or the closing one)
// follows it, and `"` as `\"`.
bool os_quote_arg(const char* in, char* out, size_t out_size){
    size_t used = 0;
    if(out_size < 3){
        return false;
    }
    out[used++] = '"';
    for(const char* c = in;; c++){
        size_t slashes = 0;
        while(*c == '\\'){
            slashes++;
            c++;
        }
        size_t copies = (*c == '"' || !*c) ? slashes * 2 : slashes;
        if(used + copies + 3 > out_size){
            return false;
        }
        memset(out + used, '\\', copies);
        used += copies;
        if(!*c){
            break;
        }
        if(*c == '"'){
            out[used++] = '\\';
        }
        out[used++] = *c;
    }
    out[used++] = '"';
    out[used] = 0;
    return true;
}

int os_run_command(const char* cmd){
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    // CreateProcessA may write to the command line.
    char* line = _strdup(cmd);
    if(!line || !CreateProcessA(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process)){
        fprintf(stderr, "cannot run %s\n", cmd);
        free(line);
        return 127;
    }
    free(line);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    if(code == 0){
        return 0;
    }
    // exit(-1) is 0xffffffff here; keep what a POSIX shell would show.
    return code <= 255 ? (int)code : 1;
}

bool os_have_command(const char* tool){
    char found[MAX_PATH];
    return SearchPathA(NULL, tool, ".exe", sizeof(found), found, NULL) > 0;
}

int os_run_forked(int (*fn)(void* arg), void* arg){
    return OS_NO_FORK;
}

FILE* os_memory_file(const char* text){
    char path[MAX_PATH + 32];
    snprintf(path, sizeof(path), "%s\\samccXXXXXX", os_temp_dir());
    if(_mktemp_s(path, strlen(path) + 1) != 0){
        return NULL;
    }
    // _O_TEMPORARY: Windows deletes the file when it is closed.
    int fd;
    if(_sopen_s(&fd, path, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_TEMPORARY, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0){
        return NULL;
    }
    size_t len = strlen(text);
    if(_write(fd, text, (unsigned)len) != (int)len || _lseek(fd, 0, SEEK_SET) != 0){
        _close(fd);
        return NULL;
    }
    FILE* file = _fdopen(fd, "rb");
    if(!file){
        _close(fd);
    }
    return file;
}

#else

#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

const char* os_temp_dir(void){
    const char* dir = getenv("TMPDIR");
    return dir && dir[0] ? dir : "/tmp";
}

int os_make_temp_file(char* path_template){
    int fd = mkstemp(path_template);
    if(fd < 0){
        return -1;
    }
    // mkstemp creates the file as 0600; give it the usual 0666 & ~umask.
    mode_t mask = umask(0);
    umask(mask);
    fchmod(fd, 0666 & ~mask);
    close(fd);
    return 0;
}

int os_make_temp_dir(char* path_template){
    return mkdtemp(path_template) ? 0 : -1;
}

int os_replace_file(const char* from, const char* to){
    return rename(from, to);
}

// readlink follows a symlink such as /usr/local/bin/samcc to the binary.
bool os_self_path(char* out, size_t out_size){
    ssize_t n = readlink("/proc/self/exe", out, out_size - 1);
    if(n <= 0){
        return false;
    }
    out[n] = 0;
    return true;
}

const char* os_base_name(const char* path){
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// Single quotes for /bin/sh, so a path reaches the assembler and linker
// as one argument and is never run as shell syntax (E2).
bool os_quote_arg(const char* in, char* out, size_t out_size){
    size_t used = 0;
    if(out_size < 3){
        return false;
    }
    out[used++] = '\'';
    for(const char* c = in; *c; c++){
        const char* piece = *c == '\'' ? "'\\''" : 0;
        size_t piece_len = piece ? strlen(piece) : 1;
        if(used + piece_len + 2 > out_size){
            return false;
        }
        if(piece){
            memcpy(out + used, piece, piece_len);
        } else {
            out[used] = *c;
        }
        used += piece_len;
    }
    out[used++] = '\'';
    out[used] = 0;
    return true;
}

int os_run_command(const char* cmd){
    int rc = system(cmd);
    if(rc == 0){
        return 0;
    }
    // system() returns a wait status; 256 truncated to an exit code
    // would be 0, so report the tool's own status (or 1).
    if(rc > 0 && WIFEXITED(rc) && WEXITSTATUS(rc) != 0){
        return WEXITSTATUS(rc);
    }
    return 1;
}

bool os_have_command(const char* tool){
    char quoted[256];
    char check_cmd[300];
    if(!os_quote_arg(tool, quoted, sizeof(quoted))){
        return false;
    }
    snprintf(check_cmd, sizeof(check_cmd), "command -v %s >/dev/null 2>&1", quoted);
    return system(check_cmd) == 0;
}

int os_run_forked(int (*fn)(void* arg), void* arg){
    const char* no_fork = getenv("SAMCC_NO_FORK");
    if(no_fork && no_fork[0]){
        return OS_NO_FORK;
    }
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if(pid < 0){
        fprintf(stderr, "cannot fork: %s\n", strerror(errno));
        return 1;
    }
    if(pid == 0){
        exit(fn(arg));
    }
    int status = 0;
    if(waitpid(pid, &status, 0) < 0){
        fprintf(stderr, "error: cannot wait for the compile: %s\n", strerror(errno));
        return 1;
    }
    // a compile that crashed made the driver exit 1 with no
    // message at all (`XSTR(TWO)` did).
    if(WIFSIGNALED(status)){
        fprintf(stderr, "error: internal compiler error: the compile was killed by signal %d\n", WTERMSIG(status));
        return 1;
    }
    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0){
        return 1;
    }
    return 0;
}

FILE* os_memory_file(const char* text){
    return fmemopen((void*)text, strlen(text), "r");
}

#endif
