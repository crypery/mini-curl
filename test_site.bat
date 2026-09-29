@echo off
echo Running tests site...
echo Test github.com ...
mini-curl.exe https://api.github.com >_github.com.txt
if %errorlevel% neq 0 (
    echo Test github.com FAILED
)
echo Test badssl.com ...
mini-curl.exe https://badssl.com >_badssl.com.txt
if %errorlevel% neq 0 (
    echo Test badssl.com FAILED
)
echo Test google.com ...
mini-curl.exe https://www.google.com >_google.com.txt
if %errorlevel% neq 0 (
    echo Test www.google.com FAILED
)
echo Test wikipedia.org ...
mini-curl.exe https://www.wikipedia.org >_wikipedia.org.txt
if %errorlevel% neq 0 (
    echo Test www.wikipedia.org FAILED
)
echo Test httpbin.org ...
mini-curl.exe https://httpbin.org >_httpbin.org.txt
if %errorlevel% neq 0 (
    echo Test httpbin.org FAILED
)
