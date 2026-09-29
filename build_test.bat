@echo off
echo Building unit tests...
gcc -O2 -DCURL_TEST_MODE -o test_curl.exe -I. -I./zlib -I./crypery ./zlib/*.c ./crypery/*.c mini-curl.c test_curl.c -lws2_32
if %errorlevel% neq 0 (
    echo Build FAILED
    exit /b 1
)
echo Running unit tests...
test_curl.exe
if %errorlevel% neq 0 (
    echo Tests FAILED
    exit /b 1
)
echo All tests passed.
