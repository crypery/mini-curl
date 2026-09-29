@echo off
echo Building Client with HTTPS support...
gcc -O2 -o mini-curl.exe -I. -I./zlib  -I./crypery ./zlib/*.c ./crypery/*.c mini-curl.c -lws2_32 -lz
if %errorlevel% neq 0 (
    echo Build FAILED
    exit /b 1
)
echo Build OK
