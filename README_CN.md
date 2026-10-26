# mini-curl

> [English version](README.md)

> [Russian version](README_RU.md)

自包含的迷你 HTTPS 客户端，采用 `curl` 风格的命令行界面，使用 C/CPP 编写。

## 项目概述

**mini-curl** 是一个命令行 HTTP/HTTPS 客户端，其界面模仿了流行的 `curl` 工具。该项目是自包含的：所有网络与加密功能均由内置的 **crypery** 库（纯 C/C++ 网络栈）提供；唯一的外部依赖是 zlib（可选，用于 gzip 解压缩）。

客户端同时支持普通 HTTP 和 HTTPS（TLS 1.2/1.3）。所有加密算法均在 crypery 库内从零实现--不使用 OpenSSL 或其他外部加密库。

## 项目特点

- HTTP/1.1 和 HTTPS（TLS 1.2/1.3）
- curl 风格的命令行界面：短选项和长选项
- 任意请求方法（`-X`）、HEAD 请求（`-I`）
- 自动跟随重定向（`-L`），可配置上限（`--max-redirs`）
- 自定义请求头（`-H`）、POST 数据（`-d`）、multipart/form-data（`-F`）
- 将响应保存到文件（`-o`、`-O`）：10 MB 以内的响应体按经典方式读取到内存，更大的（或大小未知 / chunked）响应体按块流式写入磁盘——数 GB 的文件无需全部放入内存，也绝不会打印到控制台
- 下载时按数据块计 I/O 限制：每次读取都获得全新的超时窗口，网络短暂停顿不会中断传输；只有完全静默（死亡）的链路才会超时
- 通过 Range 请求恢复中断的下载（`-C`）
- 持久化到文件的 Cookie 存储（`--cook`）
- 连接超时（`--connect-timeout`）
- 详细模式（`-v`）、静默模式（`-s`）、禁用证书校验（`-k`）
- chunked 传输编码解码
- 响应 gzip 解压缩（通过 zlib）
- 可移植性：Windows（MinGW、Winsock）和类 Unix 系统（BSD 套接字）
- 除内置的 crypery 库和 zlib 外无外部依赖
- 返回码：请求成功（2xx 状态）为 `0`，出错为 `1`

## 使用的模块

- **crypery/tls** - 基础加密层：纯 C 实现的 TLS 1.2/1.3（AES-128-GCM、SHA-256、HMAC-SHA256、HKDF、RSA-2048、ECDH P-256/X25519、X.509 证书）。
- **crypery/ssl** - 基于内置 TLS 实现的 OpenSSL 兼容 API 层：握手、套接字上的加密 I/O、超时、证书校验。
 - **crypery/url** - HTTP/HTTPS 客户端库：URL 解析、HTTP/1.1 请求、响应读取（Content-Length、chunked）、重定向、multipart 表单、文件下载到磁盘（`url_download_file()`：大响应体按块流式写盘、按数据块计 I/O 限制、Range/206 断点续传）、DNS 解析、带超时的连接。
- **zlib** - 可选库，用于 gzip 解压缩压缩后的响应。

## 编译

需要 GCC（Windows 上为 MinGW-w64）。

编译通过 `build_curl.bat` 脚本执行：

```bat
build_curl.bat
```

该脚本以 `-O2` 优化编译所有源码（zlib、crypery、mini-curl.c），生成 `mini-curl.exe`。

等效的手动命令：

 ```bat
 gcc -O2 -o mini-curl.exe -I. -I./zlib -I./crypery ./zlib/*.c ./crypery/*.c mini-curl.c -lws2_32 -lz
 ```

在类 Unix 系统上不需要 `-lws2_32` 参数。

## 使用方法

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

### 示例

```bat
:: GET 请求
mini-curl.exe https://example.com

:: 仅显示文档头
mini-curl.exe -I https://example.com

:: 跟随重定向
mini-curl.exe -L http://example.com

:: POST 数据
mini-curl.exe -d "name=value" https://httpbin.org/post

:: 自定义请求头
mini-curl.exe -H "X-Api-Key: 123" https://api.example.com

 :: 将响应保存到文件
 mini-curl.exe -o page.html https://example.com
 
 :: 下载大文件：保存到磁盘、按块流式传输（不向控制台输出任何内容），自动跟随重定向
 mini-curl.exe -L -o model.gguf https://huggingface.co/unsloth/Qwen3.5-0.8B-GGUF/resolve/main/Qwen3.5-0.8B-Q4_K_M.gguf
 
 :: 续传中断的下载（根据已有文件自动确定偏移量）
 mini-curl.exe -L -C - -o model.gguf https://huggingface.co/unsloth/Qwen3.5-0.8B-GGUF/resolve/main/Qwen3.5-0.8B-Q4_K_M.gguf
 
 :: 详细模式（输出到 stderr，包含下载进度）
 mini-curl.exe -v https://example.com

:: 连接超时 5 秒
mini-curl.exe --connect-timeout 5 https://example.com

:: Cookie：首次请求后保存到 cookies.txt，下次运行自动从文件读取（发送 Cookie 头）
mini-curl.exe --cook cookies.txt https://example.com/login
mini-curl.exe --cook cookies.txt https://example.com/profile
```

### 测试

项目中包含一个本地测试 HTTP 服务器。启动它并向其发送请求：

```bat
python test_server.py 8765
mini-curl.exe -v http://127.0.0.1:8765/
```

服务器支持以下场景：普通 GET、chunked 响应、重定向、文件下载、Range 请求（恢复下载）、Cookie（`/set-cookie`、`/whoami`）。
