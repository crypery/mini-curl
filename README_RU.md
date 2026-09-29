# mini-curl

> [English version](README.md)

> [China version](README_CN.md)

Самодостаточный мини HTTPS-клиент с командным интерфейсом в стиле `curl`, написанный на C/CPP.

## Описание проекта

**mini-curl** - командная строка HTTP/HTTPS-клиент, повторяющий интерфейс популярной утилиты `curl`. Проект самодостаточный: вся сетевая и криптографическая функциональность обеспечивается встроенной библиотекой **crypery** (сетевой стек на чистом C/C++), единственная внешняя зависимость - zlib (опционально, для gzip-распаковки).

Клиент работает как с обычным HTTP, так и с HTTPS (TLS 1.2/1.3). Все криптоалгоритмы реализованы с нуля внутри библиотеки crypery - OpenSSL и другие внешние криптографические библиотеки не используются.

## Особенности проекта

- HTTP/1.1 и HTTPS (TLS 1.2/1.3)
- Командный интерфейс в стиле curl: короткие и длинные варианты опций
- Произвольный метод запроса (`-X`), HEAD-запросы (`-I`)
- Автоматическое следование редиректам (`-L`) с настраиваемым лимитом (`--max-redirs`)
- Пользовательские заголовки (`-H`), POST-данные (`-d`), multipart/form-data (`-F`)
- Сохранение ответа в файл (`-o`, `-O`)
- Возобновление прерванной загрузки через Range-запросы (`-C`)
- Таймаут установления соединения (`--connect-timeout`)
- Подробный (verbose) режим (`-v`), тихий режим (`-s`), отключение проверки сертификата (`-k`)
- Декодирование chunked transfer encoding
- gzip-распаковка ответов (через zlib)
- Портативность: Windows (MinGW, Winsock) и Unix-подобные системы (BSD-сокеты)
- Нет внешних зависимостей, кроме встроенной библиотеки crypery и zlib
- Код возврата: `0` при успешном запросе (статус 2xx), `1` при ошибке

## Используемые модули

- **crypery/tls** - базовый криптографический слой: реализация TLS 1.2/1.3 на чистом C (AES-128-GCM, SHA-256, HMAC-SHA256, HKDF, RSA-2048, ECDH P-256/X25519, X.509-сертификаты).
- **crypery/ssl** - OpenSSL-совместимый API-слой над встроенной реализацией TLS: рукопожатие, шифрованный ввод-вывод поверх сокета, таймауты, проверка сертификата.
- **crypery/url** - HTTP/HTTPS-библиотека клиента: разбор URL, HTTP/1.1-запросы, чтение ответа (Content-Length, chunked), редиректы, multipart-формы, DNS-разрешение, подключение с таймаутом.
- **zlib** - опциональная библиотека для gzip-распаковки сжатых ответов.

## Компиляция

Требуется GCC (на Windows - MinGW-w64).

Компиляция выполняется скриптом `build_curl.bat`:

```bat
build_curl.bat
```

Скрипт компилирует все исходники (zlib, crypery, mini-curl.c) с оптимизацией `-O2` и формирует `mini-curl.exe`.

Эквивалентная команда вручную:

```bat
gcc -O2 -o mini-curl.exe -I. -I./zlib -I./crypery./zlib/*.c./crypery/*.c mini-curl.c -lws2_32 -lz
```

На Unix-подобных системах флаг `-lws2_32` не нужен.

## Использование

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
 -h, --help               This help text
     --version            Show version information and exit
```

### Примеры

```bat
:: GET-запрос
mini-curl.exe https://example.com

:: Только заголовки документа
mini-curl.exe -I https://example.com

:: Следовать редиректам
mini-curl.exe -L http://example.com

:: POST-данные
mini-curl.exe -d "name=value" https://httpbin.org/post

:: Пользовательский заголовок
mini-curl.exe -H "X-Api-Key: 123" https://api.example.com

:: Сохранить ответ в файл
mini-curl.exe -o page.html https://example.com

:: Подробный режим (вывод в stderr)
mini-curl.exe -v https://example.com

:: Таймаут соединения 5 секунд
mini-curl.exe --connect-timeout 5 https://example.com
```

### Тестирование

В проекте есть локальный тестовый HTTP-сервер. Запустите его и выполните запрос к нему:

```bat
python test_server.py 8765
mini-curl.exe -v http://127.0.0.1:8765/
```

Сервер поддерживает сценарии: обычный GET, chunked-ответы, редиректы, загрузка файла, Range-запросы (возобновление загрузки).
