# mini-curl

> [Russian version](README_RU.md)

> [China version](README_CN.md)

A self-contained mini HTTPS client with a `curl`-like command-line interface, written in C/CPP.

## Project Overview

**mini-curl** is a command-line HTTP/HTTPS client that mimics the interface of the popular `curl` utility. The project is self-contained: all networking and cryptographic functionality is provided by the bundled **crypery** library (a pure C/C++ network stack); the only external dependency is zlib (optional, for gzip decompression).

The client works with both plain HTTP and HTTPS (TLS 1.2/1.3). All cryptographic algorithms are implemented from scratch inside the crypery library - OpenSSL and other external crypto libraries are not used.

## Features

- HTTP/1.1 and HTTPS (TLS 1.2/1.3)
- curl-like command-line interface: short and long option forms
- Arbitrary request method (`-X`), HEAD requests (`-I`)
- Automatic redirect following (`-L`) with a configurable limit (`--max-redirs`)
- Custom headers (`-H`), POST data (`-d`), multipart/form-data (`-F`)
- Saving the response to a file (`-o`, `-O`): bodies up to 10 MB are fetched the classic way (into memory), larger (or size-unknown / chunked) bodies are streamed to disk chunk by chunk - multi-gigabyte files do not need to fit in RAM and are never printed to the console
- Per-chunk I/O limits on downloads: every read gets a fresh timeout window and a temporary network stall does not abort the transfer; only a fully silent (dead) link times out
- Resuming interrupted transfers via Range requests (`-C`)
- Cookie jar persisted to a file across runs (`--cook`)
- Connection timeout (`--connect-timeout`)
- Verbose mode (`-v`), silent mode (`-s`), disabling certificate verification (`-k`)
- Chunked transfer encoding decoding
- gzip decompression of responses (via zlib)
- Portability: Windows (MinGW, Winsock) and Unix-like systems (BSD sockets)
- No external dependencies besides the bundled crypery library and zlib
- Exit code: `0` on a successful request (2xx status), `1` on error

## Modules Used

- **crypery/tls** - the base cryptographic layer: a pure C implementation of TLS 1.2/1.3 (AES-128-GCM, SHA-256, HMAC-SHA256, HKDF, RSA-2048, ECDH P-256/X25519, X.509 certificates).
- **crypery/ssl** - an OpenSSL-compatible API layer over the built-in TLS implementation: handshake, encrypted I/O over a socket, timeouts, certificate verification.
 - **crypery/url** - the HTTP/HTTPS client library: URL parsing, HTTP/1.1 requests, response reading (Content-Length, chunked), redirects, multipart forms, file downloads to disk (`url_download_file()`: streamed chunk by chunk for large bodies, per-chunk I/O limits, resume via Range/206), DNS resolution, connection with timeout.
- **zlib** - an optional library for gzip decompression of compressed responses.

## Building

GCC is required (MinGW-w64 on Windows).

The build is performed by the `build_curl.bat` script:

```bat
build_curl.bat
```

The script compiles all sources (zlib, crypery, mini-curl.c) with `-O2` optimization and produces `mini-curl.exe`.

Equivalent manual command:

 ```bat
 gcc -O2 -o mini-curl.exe -I. -I./zlib -I./crypery ./zlib/*.c ./crypery/*.c mini-curl.c -lws2_32 -lz
 ```

On Unix-like systems the `-lws2_32` flag is not needed.

## Usage

```
Usage: mini-curl [options...] <url>

Options:
 -X, --request METHOD     Specify request method to use
 -I, --head               Show document info only (HEAD)
 -L, --location           Follow redirects
 -H, --header LINE        Pass custom header(s) to server
 -d, --data DATA          HTTP POST data
 -F, --form CONTENT       Specify multipart MIME data
 -o, --output FILE        Write to file instead of stdout
 -O, --remote-name        Write output to a file named as the remote file
 -C, --continue-at OFFSET Resume a transfer at the given offset ("-" = auto)
 -v, --verbose            Make the operation more talkative
 -k, --insecure           Allow insecure server connections when using SSL
 -s, --silent             Silent mode. Don't output anything
     --connect-timeout SECONDS  Maximum time allowed for connection
     --max-redirs NUM     Maximum number of redirects allowed (default 50)
     --cook FILE          Read cookies from FILE and save updated cookies back
  -h, --help               This help text
     --version            Show version information and exit
```

### Examples

```bat
:: GET request
mini-curl.exe https://example.com

:: Document headers only
mini-curl.exe -I https://example.com

:: Follow redirects
mini-curl.exe -L http://example.com

:: POST data
mini-curl.exe -d "name=value" https://httpbin.org/post

:: Custom header
mini-curl.exe -H "X-Api-Key: 123" https://api.example.com

 :: Save the response to a file
 mini-curl.exe -o page.html https://example.com
 
 :: Download a large file: saved to disk, streamed chunk by chunk
 :: (nothing is printed to the console), redirects are followed
 mini-curl.exe -L -o model.gguf https://huggingface.co/unsloth/Qwen3.5-0.8B-GGUF/resolve/main/Qwen3.5-0.8B-Q4_K_M.gguf
 
 :: Resume an interrupted download (auto offset from the existing file)
 mini-curl.exe -L -C - -o model.gguf https://huggingface.co/unsloth/Qwen3.5-0.8B-GGUF/resolve/main/Qwen3.5-0.8B-Q4_K_M.gguf
 
 :: Verbose mode (output to stderr, download progress included)
 mini-curl.exe -v https://example.com

:: 5-second connection timeout
mini-curl.exe --connect-timeout 5 https://example.com

:: Cookie jar: cookies are saved to cookies.txt after the first request
:: and picked up from the file on the next run (Cookie header is sent)
mini-curl.exe --cook cookies.txt https://example.com/login
mini-curl.exe --cook cookies.txt https://example.com/profile
```

### Testing

The project includes a local test HTTP server. Start it and send a request to it:

```bat
python test_server.py 8765
mini-curl.exe -v http://127.0.0.1:8765/
```

The server supports the following scenarios: plain GET, chunked responses, redirects, file download, Range requests (transfer resumption), cookies (`/set-cookie`, `/whoami`).
