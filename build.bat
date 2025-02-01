@echo off
rem Windows build with MinGW-w64: gcc from MSYS2's
rem ucrt64 or mingw64 environment, or any x86_64-w64-mingw32-gcc, on the
rem PATH. Set CC to use another one. Builds main.exe here; pc_includes
rem must stay next to it.
setlocal
if "%CC%"=="" set CC=gcc
cd /d "%~dp0"
"%CC%" -g -Wall -Wextra -Wno-unused-parameter -I. -o main.exe ^
    main.c os.c compiler.c cprocess.c validator.c lexer.c lex_process.c token.c ^
    parser.c node.c expressionable.c datatype.c scope.c symresolver.c resolver.c ^
    rdefault.c codegen.c codegen_x86.c codegen_x86_64.c codegen_aarch64.c ^
    codegen_riscv64.c target.c stackframe.c fixup.c array.c initializer.c exp64.c ^
    helper.c constant.c atomic.c preprocessor/preprocessor.c preprocessor/native.c ^
    preprocessor/static-include.c preprocessor/static-includes/stdarg.c ^
    preprocessor/static-includes/stddef.c helpers/buffer.c helpers/vector.c ^
    helpers/arena.c
if errorlevel 1 exit /b 1
echo built main.exe
