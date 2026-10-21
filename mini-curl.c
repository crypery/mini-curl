/*
 * Git: https://github.com/crypery
 * Author: https://crypery.com
 * License: GNU AGPL v3 (Affero GPL)
 */

/*
 * mini-curl.c - curl-like HTTP/HTTPS command-line client
 *
 * A self-contained HTTP/HTTPS client with a curl-like interface.
 * Supports: -X/-I/-L, -H/-d/-F, -o/-O/-C, -v/-k/-s, --connect-timeout,
 * --cook.
 *
 * Features:
 *   - HTTP/1.1 with persistent connection close
 *   - HTTPS via bundled TLS stack (ssl.c + tls.c)
 *   - Automatic redirect following (-L)
 *   - Chunked transfer decoding
 *   - Multipart form data uploads (-F)
 *   - Range requests / resume (-C)
 *   - Custom headers (-H)
 *   - Cookie jar persisted to a file across runs (--cook)
 *
 * The actual HTTP/HTTPS work is done by the url library (url.c / url.h):
 *
 *     UrlCtx *ctx = url_init();
 *     ctx->verbose =...;                 // log on/off
 *     url_set_header(ctx,...);           // headers if needed
 *     url_set_data(ctx,...);             // POST data if needed
 *     char *data = url_run(ctx, url, method);
 *     url_free(ctx);
 *
 * No external libraries are used besides the bundled ssl.c / tls.c / url.c and
 * the standard C library + sockets. The transport uses Winsock + WinAPI on
 * Windows and BSD sockets on POSIX (see url.c / ssl.c).
 */

#include "mini-curl.h"
#include "url.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Get option value from inline string or next argument
static const char *opt_need_value(int argc, char **argv, int *i, const char *inlinev) {
    if (inlinev && *inlinev) return inlinev;
    if (*i + 1 < argc) { (*i)++; return argv[*i]; }
    return NULL;
}

// Set and validate HTTP request method
static int opt_set_method(Options *o, const char *m) {
    if (!*m) {
        fprintf(stderr, "curl: option --request: empty method\n");
        return -1;
    }
    for (const char *p = m; *p; p++)
        if (!isalnum((unsigned char)*p)) {
            fprintf(stderr, "curl: option --request: invalid method '%s'\n", m);
            return -1;
        }
    if (strlen(m) >= sizeof(o->method)) {
        fprintf(stderr, "curl: option --request: method too long\n");
        return -1;
    }
    snprintf(o->method, sizeof(o->method), "%s", m);
    o->method_set = 1;
    return 0;
}

// Add a custom header to options
static int opt_add_header(Options *o, const char *v) {
    const char *colon = strchr(v, ':');
    if (!colon) {
        fprintf(stderr, "curl: option --header: invalid header '%s'\n", v);
        return -1;
    }
    char name[64];
    size_t nlen = (size_t)(colon - v);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, v, nlen);
    name[nlen] = '\0';
    url_header_add(&o->user_headers, name, colon + 1);
    return 0;
}

// Parse command-line options for curl-like interface
int parse_options(int argc, char **argv, Options *o, int *url_idx) {
    memset(o, 0, sizeof *o);
    o->max_redirs = URL_MAX_REDIRS;
    o->connect_timeout_ms = -1;
    o->resume_offset = -1;
    *url_idx = -1;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--") == 0) {
            if (*url_idx < 0 && i + 1 < argc) *url_idx = i + 1;
            break;
        }

        if (arg[0] == '-' && arg[1] == '-') {
            char name[64];
            const char *eq = strchr(arg, '=');
            const char *val = NULL;
            size_t nlen = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
            memcpy(name, arg + 2, nlen);
            name[nlen] = '\0';
            if (eq) val = eq + 1;

            if (strcmp(name, "request") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val || opt_set_method(o, val) != 0) return -1;
            } else if (strcmp(name, "head") == 0) {
                o->head = 1;
            } else if (strcmp(name, "location") == 0) {
                o->location = 1;
            } else if (strcmp(name, "header") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val || opt_add_header(o, val) != 0) return -1;
            } else if (strcmp(name, "data") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                if (o->data_count < URL_MAX_DATA) o->data[o->data_count++] = val;
            } else if (strcmp(name, "form") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                if (o->form_count < URL_MAX_FORMS) o->forms[o->form_count++] = val;
            } else if (strcmp(name, "insecure") == 0) {
                o->insecure = 1;
            } else if (strcmp(name, "silent") == 0) {
                o->silent = 1;
            } else if (strcmp(name, "user-agent") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                snprintf(o->user_agent, sizeof(o->user_agent), "%s", val);
            } else if (strcmp(name, "verbose") == 0) {
                o->verbose = 1;
            } else if (strcmp(name, "output") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                o->outfile = val;
            } else if (strcmp(name, "remote-name") == 0) {
                o->remote_name = 1;
            } else if (strcmp(name, "continue-at") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                if (strcmp(val, "-") == 0) {
                    o->resume = 1;
                    o->resume_offset = -1;
                } else {
                    char *end;
                    long off = strtol(val, &end, 10);
                    if (*end || off < 0) {
                        fprintf(stderr, "curl: option --continue-at: invalid offset '%s'\n", val);
                        return -1;
                    }
                    o->resume = 1;
                    o->resume_offset = off;
                }
            } else if (strcmp(name, "connect-timeout") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                char *end;
                double secs = strtod(val, &end);
                if (*end || secs < 0) {
                    fprintf(stderr, "curl: option --connect-timeout: invalid value '%s'\n", val);
                    return -1;
                }
                o->connect_timeout_set = 1;
                o->connect_timeout_ms = (long)(secs * 1000.0);
            } else if (strcmp(name, "max-redirs") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                char *end;
                long n = strtol(val, &end, 10);
                if (*end || n < 0) {
                    fprintf(stderr, "curl: option --max-redirs: invalid value '%s'\n", val);
                    return -1;
                }
                o->max_redirs = (int)n;
            } else if (strcmp(name, "cook") == 0) {
                val = opt_need_value(argc, argv, &i, val);
                if (!val) return -1;
                o->cook_file = val;
            } else {
                fprintf(stderr, "curl: option --%s is unknown\n", name);
                return -1;
            }
        } else if (arg[0] == '-' && arg[1] && arg[1] != '-') {
            const char *p = arg + 1;
            while (*p) {
                int consume_rest = 0;
                switch (*p) {
                case 'X': {
                    const char *v = opt_need_value(argc, argv, &i, p + 1);
                    if (!v || opt_set_method(o, v) != 0) return -1;
                    consume_rest = 1;
                    break;
                }
                case 'I': o->head = 1; break;
                case 'L': o->location = 1; break;
                case 'H': {
                    const char *v = opt_need_value(argc, argv, &i, p + 1);
                    if (!v || opt_add_header(o, v) != 0) return -1;
                    consume_rest = 1;
                    break;
                }
                case 'd': {
                    const char *v = opt_need_value(argc, argv, &i, p + 1);
                    if (!v) return -1;
                    if (o->data_count < URL_MAX_DATA) o->data[o->data_count++] = v;
                    consume_rest = 1;
                    break;
                }
                case 'F': {
                    const char *v = opt_need_value(argc, argv, &i, p + 1);
                    if (!v) return -1;
                    if (o->form_count < URL_MAX_FORMS) o->forms[o->form_count++] = v;
                    consume_rest = 1;
                    break;
                }
                case 'o':
                    o->outfile = opt_need_value(argc, argv, &i, p + 1);
                    if (!o->outfile) return -1;
                    consume_rest = 1;
                    break;
                case 'O': o->remote_name = 1; break;
                case 'C': {
                    const char *v = opt_need_value(argc, argv, &i, p + 1);
                    if (!v) return -1;
                    if (strcmp(v, "-") == 0) {
                        o->resume = 1;
                        o->resume_offset = -1;
                    } else {
                        char *end;
                        long off = strtol(v, &end, 10);
                        if (*end || off < 0) {
                            fprintf(stderr, "curl: option -C: invalid offset '%s'\n", v);
                            return -1;
                        }
                        o->resume = 1;
                        o->resume_offset = off;
                    }
                    consume_rest = 1;
                    break;
                }
                case 'v': o->verbose = 1; break;
                case 'A': {
                    const char *ua = opt_need_value(argc, argv, &i, p + 1);
                    if (!ua) return -1;
                    snprintf(o->user_agent, sizeof(o->user_agent), "%s", ua);
                    consume_rest = 1;
                    break;
                }
                case 'k': o->insecure = 1; break;
                case 's': o->silent = 1; break;
                default:
                    fprintf(stderr, "curl: option -%c is unknown\n", *p);
                    return -1;
                }
                if (consume_rest) break;
                p++;
            }
        } else {
            /* Positional argument: treat it as a URL. curl accepts options
             * both before and after the URL, so keep parsing. */
            if (*url_idx < 0) *url_idx = i;
        }
    }
    return 0;
}

// Determine HTTP method based on options
int choose_method(const Options *o, char *out, size_t outlen) {
    if (o->method_set) { snprintf(out, outlen, "%s", o->method); return 0; }
    if (o->head) { snprintf(out, outlen, "HEAD"); return 0; }
    if (o->form_count || o->data_count) { snprintf(out, outlen, "POST"); return 0; }
    snprintf(out, outlen, "GET");
    return 0;
}

// Print version information
static void print_version(void) {
#ifdef _WIN32
    printf("mini-curl (Windows) self-contained TLS (1.2/1.3)\n");
#else
    printf("mini-curl (POSIX) self-contained TLS (1.2/1.3)\n");
#endif
    printf("Features: TLS, HTTP, redirects, multipart forms\n");
}

// Print help text to file
static void print_help(FILE *f) {
    fprintf(f,
        "Usage: curl [options...] <url>\n"
        "\n"
        "Options:\n"
        " -X, --request METHOD     Specify request method to use\n"
        " -I, --head               Show document info only (HEAD)\n"
        " -L, --location           Follow redirects\n"
        " -H, --header LINE        Pass custom header(s) to server\n"
        " -d, --data DATA          HTTP POST data\n"
        " -F, --form CONTENT       Specify multipart MIME data\n"
        " -o, --output FILE        Write to file instead of stdout\n"
        " -O, --remote-name        Write output to a file named as the remote file\n"
        " -C, --continue-at OFFSET Resume a transfer at the given offset (\"-\" = auto)\n"
        " -v, --verbose            Make the operation more talkative\n"
        " -k, --insecure           Allow insecure server connections when using SSL\n"
        " -s, --silent             Silent mode. Don't output anything\n"
         "     --connect-timeout SECONDS  Maximum time allowed for connection\n"
         "     --max-redirs NUM     Maximum number of redirects allowed (default %d)\n"
         "     --cook FILE          Read cookies from FILE and save updated cookies back\n"
         " -h, --help               This help text\n"
        "     --version            Show version information and exit\n",
        URL_MAX_REDIRS);
}

// Get file size in bytes
static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}

// Determine output path for range requests
static int output_path_for_range(const Options *o, const UrlInfo *u, char *out, size_t outlen) {
    if (o->outfile) { snprintf(out, outlen, "%s", o->outfile); return 0; }
    if (o->remote_name) { url_remote_name(u, NULL, out, outlen); return 0; }
    return -1;
}

#ifndef CURL_TEST_MODE

// Log callback for url_run(): writes the formatted log lines to stderr.
static void log_line(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
    }

    Options o;
    int url_idx = -1;
    if (parse_options(argc, argv, &o, &url_idx) != 0) {
        fprintf(stderr, "Try 'curl --help' for more information.\n");
        return 2;
    }
    if (url_idx < 0) {
        print_help(stderr);
        return 2;
    }
    const char *url = argv[url_idx];

    // Build the request context for the url library.
    UrlCtx *ctx = url_init();
    if (!ctx) {
        fprintf(stderr, "curl: out of memory\n");
        return 1;
    }
    ctx->verbose = o.verbose;
    ctx->silent = o.silent;
    ctx->insecure = o.insecure;
    ctx->follow_redirects = o.location;
    ctx->max_redirs = o.max_redirs;
    ctx->connect_timeout_set = o.connect_timeout_set;
    ctx->connect_timeout_ms = o.connect_timeout_ms;
    if (o.user_agent[0]) url_set_user_agent(ctx, o.user_agent);
    for (int i = 0; i < o.user_headers.n; i++)
        url_set_header(ctx, o.user_headers.h[i].name, o.user_headers.h[i].value);

    // Cookie jar: load from the file on the first url_run() (if it exists)
    // and save back on every Set-Cookie received.
    if (o.cook_file) {
        ctx->use_cook = true;
        ctx->cook_file = (char *)o.cook_file;
    }

    char method[16];
    choose_method(&o, method, sizeof(method));

    // Prepare the request body (multipart form or url-encoded data).
    if (o.form_count) {
        char boundary[48] = { 0 };
        size_t blen = 0;
        char *body = NULL;
        if (url_multipart_build(o.forms, o.form_count, &body, &blen,
                                boundary, sizeof(boundary)) != 0) {
            if (!o.silent) fprintf(stderr, "curl: (26) failed to build form data\n");
            url_free(ctx);
            return 1;
        }
        ctx->data = body;          // ownership moves to the context
        ctx->data_len = blen;
        char ct[300];
        snprintf(ct, sizeof(ct), "multipart/form-data; boundary=%s", boundary);
        url_set_content_type(ctx, ct);
    } else if (o.data_count) {
        size_t total = 0;
        for (int i = 0; i < o.data_count; i++) total += strlen(o.data[i]);
        if (o.data_count > 1) total += (size_t)o.data_count - 1;
        char *body = (char *)malloc(total + 1);
        if (!body) {
            fprintf(stderr, "curl: out of memory\n");
            url_free(ctx);
            return 1;
        }
        char *p = body;
        for (int i = 0; i < o.data_count; i++) {
            if (i) *p++ = '&';
            size_t l = strlen(o.data[i]);
            memcpy(p, o.data[i], l);
            p += l;
        }
        *p = '\0';
        ctx->data = body;          // ownership moves to the context
        ctx->data_len = total;
        url_set_content_type(ctx, "application/x-www-form-urlencoded");
    }

    // Range header for -C/--continue-at.
    if (o.resume) {
        UrlInfo u;
        if (url_parse(url, &u) == 0) {
            char rpath[1024];
            if (output_path_for_range(&o, &u, rpath, sizeof(rpath)) == 0) {
                long rstart = (o.resume_offset >= 0) ? o.resume_offset : file_size(rpath);
                if (rstart > 0) {
                    char range[64];
                    snprintf(range, sizeof(range), "bytes=%ld-", rstart);
                    url_set_range(ctx, range);
                }
            }
        }
    }

    // Run the request through the url library.
    char *data = url_run(ctx, url, method, log_line);
    int result = 1;
    if (!data) {
        result = 1;
    } else {
        size_t data_len = ctx->result_len;
        if (o.outfile || o.remote_name) {
            char outpath[1024];
            if (o.outfile) {
                snprintf(outpath, sizeof(outpath), "%s", o.outfile);
            } else {
                UrlInfo u;
                if (url_parse(url, &u) == 0)
                    url_remote_name(&u, url_response_header(ctx, "content-disposition"),
                                    outpath, sizeof(outpath));
                else
                    snprintf(outpath, sizeof(outpath), "index.html");
            }
            int append = (o.resume && url_status(ctx) == 206);
            FILE *fp = fopen(outpath, append ? "ab" : "wb");
            if (!fp) {
                if (!o.silent) fprintf(stderr, "curl: (23) failed to open file '%s'\n", outpath);
            } else {
                if (data_len > 0) fwrite(data, 1, data_len, fp);
                fclose(fp);
                if (o.verbose) fprintf(stderr, "* Saved output to %s\n", outpath);
            }
        } else {
            if (data_len > 0) fwrite(data, 1, data_len, stdout);
            fflush(stdout);
        }
        int st = url_status(ctx);
        result = (st >= 200 && st < 300) ? 0 : 1;
        free(data);
    }
    url_free(ctx);
    return result;
}

#endif // CURL_TEST_MODE
