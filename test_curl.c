/*
 * Git: https://github.com/crypery
 * Author: https://crypery.com
 * License: GNU AGPL v3 (Affero GPL)
 */

/*
 * test_curl.c - unit tests for the pure, network-independent parts of the
 * url library (url.c) and the option parsing of mini-curl.c.
 */

#include "mini-curl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static int contains(const char *hay, const char *needle) {
    return strstr(hay, needle) != NULL;
}

#define CHECK(cond, name)                                                \
    do {                                                                 \
        if (cond) printf("  PASS %s\n", name);                           \
        else { printf("  FAIL %s (line %d)\n", name, __LINE__); g_fail = 1; } \
    } while (0)

#define CHECK_EQ_INT(a, b, name) CHECK((a) == (b), name)
#define CHECK_STR(a, b, name) CHECK(a && b && strcmp(a, b) == 0, name)

static void test_url_parse(void) {
    printf("[url_parse]\n");
    UrlInfo u;
    CHECK(url_parse("https://example.com", &u) == 0, "https basic");
    CHECK(u.is_ssl == 1, "https is_ssl");
    CHECK_STR(u.host, "example.com", "https host");
    CHECK_EQ_INT(u.port, 443, "https default port");
    CHECK_STR(u.path, "/", "https default path");

    CHECK(url_parse("http://example.com/path?q=1#frag", &u) == 0, "http query frag");
    CHECK(u.is_ssl == 0, "http is_ssl");
    CHECK_EQ_INT(u.port, 80, "http default port");
    CHECK_STR(u.path, "/path?q=1", "path keeps query, strips fragment");

    CHECK(url_parse("https://example.com:8080/x", &u) == 0, "explicit port");
    CHECK_EQ_INT(u.port, 8080, "port parsed");
    CHECK_STR(u.path, "/x", "path");

    CHECK(url_parse("https://user:pass@example.com/x", &u) == 0, "userinfo");
    CHECK_STR(u.host, "example.com", "userinfo stripped");

    CHECK(url_parse("http://[::1]:8080/x", &u) == 0, "ipv6 literal");
    CHECK_STR(u.host, "::1", "ipv6 host");
    CHECK_EQ_INT(u.port, 8080, "ipv6 port");

    CHECK(url_parse("ftp://example.com", &u) != 0, "bad scheme rejected");
    CHECK(url_parse("not a url", &u) != 0, "garbage rejected");
}

static void test_context(void) {
    printf("[url_init / url_set_* / url_free]\n");
    UrlCtx *ctx = url_init();
    CHECK(ctx != NULL, "url_init returns ctx");
    CHECK(url_status(ctx) == 0, "initial status 0");
    CHECK(url_response_header(ctx, "x") == NULL, "no response header");

    url_set_header(ctx, "X-Test", "1");
    CHECK_STR(url_header_get(&ctx->headers, "x-test"), "1", "set header");

    url_set_data(ctx, "a=1", 3);
    CHECK(ctx->data && ctx->data_len == 3 && memcmp(ctx->data, "a=1", 3) == 0, "set data copies");

    url_set_content_type(ctx, "application/json");
    CHECK_STR(ctx->content_type, "application/json", "set content type");

    url_set_range(ctx, "bytes=0-");
    CHECK_STR(ctx->range, "bytes=0-", "set range");

    url_set_user_agent(ctx, "ua/1");
    CHECK_STR(ctx->user_agent, "ua/1", "set ua");
    url_free(ctx);

    UrlCtx *c2 = url_init();
    c2->silent = 1;
    char *d = url_run(c2, "not a url", "GET", NULL);
    CHECK(d == NULL, "garbage url fails");
    CHECK(c2->last_error == URL_ERR_INVALID_URL, "error code recorded");
    url_free(c2);
}

static void test_url_join(void) {
    printf("[url_join]\n");
    char out[4096];

    url_join("http://example.com/a/b", "/c", out, sizeof(out));
    CHECK_STR(out, "http://example.com/c", "root-relative");

    url_join("http://example.com/a/b", "c", out, sizeof(out));
    CHECK_STR(out, "http://example.com/a/c", "relative");

    url_join("http://example.com/a/b/", "c", out, sizeof(out));
    CHECK_STR(out, "http://example.com/a/b/c", "relative dir");

    url_join("http://example.com", "c", out, sizeof(out));
    CHECK_STR(out, "http://example.com/c", "relative no path");

    url_join("http://example.com/a/b", "https://other.com/x", out, sizeof(out));
    CHECK_STR(out, "https://other.com/x", "absolute");

    url_join("https://example.com/a/b", "//other.com/x", out, sizeof(out));
    CHECK_STR(out, "https://other.com/x", "scheme-relative");

    url_join("https://example.com:8443/x", "y", out, sizeof(out));
    CHECK_STR(out, "https://example.com:8443/y", "port preserved");
}

static void test_url_decode(void) {
    printf("[url_decode]\n");
    char s[64];
    strcpy(s, "a%20b%2Fc%41");
    url_decode(s);
    CHECK_STR(s, "a b/cA", "percent decoding");
    strcpy(s, "plain");
    url_decode(s);
    CHECK_STR(s, "plain", "no changes");
}

static void test_http_parse(void) {
    printf("[url_http_parse_status_and_headers]\n");
    const char *raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Location: /new\r\n"
        "\r\n";
    ResponseHead rh;
    CHECK(url_http_parse_status_and_headers(raw, strlen(raw), &rh) == 0, "parse ok");
    CHECK_EQ_INT(rh.status, 200, "status 200");
    CHECK_STR(rh.version, "1.1", "version");
    CHECK_STR(rh.reason, "OK", "reason");
    const char *ct = url_header_get(&rh.headers, "Content-Type");
    CHECK_STR(ct, "text/html", "content-type");
    CHECK_STR(url_header_get(&rh.headers, "location"), "/new", "location (case-insens)");

    const char *raw2 = "HTTP/1.1 302 Found\r\nlocation: /x\r\nlocation: /y\r\n\r\n";
    ResponseHead rh2;
    CHECK(url_http_parse_status_and_headers(raw2, strlen(raw2), &rh2) == 0, "parse dup");
    CHECK_EQ_INT(rh2.status, 302, "status 302");
    CHECK_STR(url_header_get(&rh2.headers, "Location"), "/y", "last wins");

    CHECK(url_http_parse_status_and_headers("NOT-HTTP/1.1 x\r\n\r\n", 22, &rh) != 0, "bad status line");
}

static void test_chunk_parse(void) {
    printf("[url_http_chunk_size]\n");
    CHECK_EQ_INT((int)url_http_chunk_size("1A"), 26, "hex 1A");
    CHECK_EQ_INT((int)url_http_chunk_size("4;ext=1"), 4, "extension ignored");
    CHECK_EQ_INT((int)url_http_chunk_size("0"), 0, "zero");
    CHECK(url_http_chunk_size("xyz") == UINT64_MAX, "invalid");
    CHECK(url_http_chunk_size("") == UINT64_MAX, "empty invalid");
}

static void test_chunk_decode(void) {
    printf("[url_http_chunked_decode]\n");
    char out[256];
    size_t out_len = 0;
    const char *body = "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n";
    CHECK(url_http_chunked_decode(body, strlen(body), out, sizeof(out), &out_len) == 0, "decode ok");
    out[out_len] = '\0';
    CHECK_STR(out, "Wikipedia", "decoded content");

    const char *body2 = "4\r\nWiki\r\n0\r\nX-Foo: y\r\n\r\n";
    out_len = 0;
    CHECK(url_http_chunked_decode(body2, strlen(body2), out, sizeof(out), &out_len) == 0, "trailers ok");
    out[out_len] = '\0';
    CHECK_STR(out, "Wiki", "trailer content");

    const char *bad = "4\r\nWik"; /* only 3 of 4 promised bytes */
    CHECK(url_http_chunked_decode(bad, strlen(bad), out, sizeof(out), &out_len) != 0, "truncated fails");
}

static void test_parse_options(void) {
    printf("[parse_options]\n");
    Options o;
    int idx = -1;

    char *a1[] = { "mini-curl", "-X", "POST", "-H", "X-Api: 1", "-d", "a=1",
                   "-o", "out.txt", "-L", "-k", "-s", "-v", "http://example.com" };
    CHECK(parse_options(14, a1, &o, &idx) == 0, "options parsed");
    CHECK_EQ_INT(idx, 13, "url index");
    CHECK(o.method_set && strcmp(o.method, "POST") == 0, "method POST");
    CHECK_EQ_INT(o.data_count, 1, "one -d");
    CHECK_STR(o.data[0], "a=1", "data value");
    CHECK_STR(o.outfile, "out.txt", "outfile");
    CHECK(o.location && o.insecure && o.silent && o.verbose, "flags set");
    CHECK_STR(url_header_get(&o.user_headers, "x-api"), "1", "user header");
    CHECK_EQ_INT(o.max_redirs, 50, "default max redirs");

    char *a2[] = { "mini-curl", "--request=DELETE", "--connect-timeout", "5",
                   "--max-redirs=3", "--head", "https://x.com" };
    idx = -1;
    CHECK(parse_options(7, a2, &o, &idx) == 0, "long options");
    CHECK_STR(o.method, "DELETE", "DELETE method");
    CHECK(o.method_set, "method set");
    CHECK_EQ_INT((int)o.connect_timeout_ms, 5000, "connect timeout 5s");
    CHECK(o.connect_timeout_set, "timeout set");
    CHECK_EQ_INT(o.max_redirs, 3, "max redirs 3");
    CHECK(o.head, "head flag");
    CHECK_EQ_INT(idx, 6, "long url index");

    char *a3[] = { "mini-curl", "-ksvL", "http://x" };
    idx = -1;
    CHECK(parse_options(3, a3, &o, &idx) == 0, "combined shorts");
    CHECK(o.insecure && o.silent && o.verbose && o.location, "all combined flags");
    CHECK_EQ_INT(idx, 2, "combined url index");

    char *a4[] = { "mini-curl", "-XPOST", "-d", "b=2", "-C", "-", "http://x" };
    idx = -1;
    CHECK(parse_options(6, a4, &o, &idx) == 0, "inline + continue");
    CHECK_STR(o.method, "POST", "inline method");
    CHECK_STR(o.data[0], "b=2", "inline data");
    CHECK(o.resume && o.resume_offset == -1, "auto resume");

    char *a5[] = { "mini-curl", "-z", "http://x" };
    CHECK(parse_options(3, a5, &o, &idx) != 0, "unknown short fails");

    char *a6[] = { "mini-curl", "--bogus", "http://x" };
    CHECK(parse_options(3, a6, &o, &idx) != 0, "unknown long fails");

    char *a7[] = { "mini-curl", "http://x", "-v", "-o", "out.txt", "-H", "X-Z: 9" };
    idx = -1;
    CHECK(parse_options(7, a7, &o, &idx) == 0, "options after URL");
    CHECK_EQ_INT(idx, 1, "url index with trailing options");
    CHECK(o.verbose, "verbose after URL");
    CHECK_STR(o.outfile, "out.txt", "outfile after URL");
    CHECK_STR(url_header_get(&o.user_headers, "x-z"), "9", "header after URL");

    char *a8[] = { "mini-curl", "--cook", "cookies.txt", "http://x" };
    idx = -1;
    CHECK(parse_options(4, a8, &o, &idx) == 0, "cook option parsed");
    CHECK_STR(o.cook_file, "cookies.txt", "cook file value");
    CHECK_EQ_INT(idx, 3, "cook url index");

    char *a9[] = { "mini-curl", "--cook=jar.txt", "http://x" };
    idx = -1;
    CHECK(parse_options(3, a9, &o, &idx) == 0, "cook inline form");
    CHECK_STR(o.cook_file, "jar.txt", "cook inline value");
}

static void test_cookies(void) {
    printf("[cookie jar]\n");

    url_cookie_set_from_url("http://example.com/login", "sid=abc; Path=/");
    char *ck = url_cookie_header_for("http://example.com/profile");
    CHECK(ck != NULL, "cookie applies to same host");
    CHECK_STR(ck, "sid=abc", "cookie header value");
    free(ck);

    char *ck2 = url_cookie_header_for("http://other.com/x");
    CHECK(ck2 == NULL, "no cross-host leak");
    free(ck2);

    url_cookie_set_from_url("http://example.com/x", "sid=; Max-Age=0");
    char *ck3 = url_cookie_header_for("http://example.com/y");
    CHECK(ck3 == NULL, "deleted cookie not sent");
    free(ck3);

    url_cookie_set_from_url("https://example.com/secure", "sec=1; Secure; Path=/");
    char *ck4 = url_cookie_header_for("http://example.com/secure");
    CHECK(ck4 == NULL, "secure cookie not sent over http");
    free(ck4);
    char *ck5 = url_cookie_header_for("https://example.com/secure");
    CHECK(ck5 && contains(ck5, "sec=1"), "secure cookie sent over https");
    free(ck5);

    url_cookie_set_from_url("http://example.com/a", "tok=xyz; Path=/");
    url_cookie_save("test_cookies.txt");
    int found = 0;
    FILE *f = fopen("test_cookies.txt", "r");
    CHECK(f != NULL, "jar file created");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f))
            if (strstr(line, "tok\t") && strstr(line, "xyz")) found = 1;
        fclose(f);
    }
    CHECK(found, "jar file contains cookie");

    f = fopen("test_cookies.txt", "w");
    CHECK(f != NULL, "jar file rewritten");
    if (f) {
        fputs("example.com\t/\tnewck\tv1\t0\t0\n", f);
        fclose(f);
    }
    url_cookie_load("test_cookies.txt");
    char *ck6 = url_cookie_header_for("http://example.com/z");
    CHECK(ck6 && contains(ck6, "newck=v1"), "loaded cookie sent");
    free(ck6);
    remove("test_cookies.txt");
}

static void test_choose_method(void) {
    printf("[choose_method]\n");
    Options o;
    char m[16];
    memset(&o, 0, sizeof(o));
    choose_method(&o, m, sizeof(m));
    CHECK_STR(m, "GET", "default GET");

    o.head = 1;
    choose_method(&o, m, sizeof(m));
    CHECK_STR(m, "HEAD", "HEAD with -I");

    o.head = 0;
    o.data_count = 1;
    choose_method(&o, m, sizeof(m));
    CHECK_STR(m, "POST", "POST with -d");

    memset(&o, 0, sizeof(o));
    o.form_count = 1;
    choose_method(&o, m, sizeof(m));
    CHECK_STR(m, "POST", "POST with -F");

    memset(&o, 0, sizeof(o));
    o.method_set = 1;
    strcpy(o.method, "PUT");
    o.data_count = 1;
    choose_method(&o, m, sizeof(m));
    CHECK_STR(m, "PUT", "-X overrides");
}

static void test_url_redirect_method(void) {
    printf("[url_redirect_method]\n");
    char m[16];
    url_redirect_method("POST", 301, m, sizeof(m));
    CHECK_STR(m, "GET", "301 POST -> GET");
    url_redirect_method("POST", 303, m, sizeof(m));
    CHECK_STR(m, "GET", "303 POST -> GET");
    url_redirect_method("POST", 307, m, sizeof(m));
    CHECK_STR(m, "POST", "307 keeps POST");
    url_redirect_method("POST", 308, m, sizeof(m));
    CHECK_STR(m, "POST", "308 keeps POST");
    url_redirect_method("GET", 302, m, sizeof(m));
    CHECK_STR(m, "GET", "302 GET stays GET");
    url_redirect_method("HEAD", 303, m, sizeof(m));
    CHECK_STR(m, "HEAD", "303 HEAD stays HEAD");
}

static void test_multipart(void) {
    printf("[url_multipart_build]\n");
    char boundary[48];
    char *out = NULL;
    size_t outlen = 0;

    const char *forms1[] = { "name=hello" };
    CHECK(url_multipart_build(forms1, 1, &out, &outlen, boundary, sizeof(boundary)) == 0, "multipart build");
    CHECK(out && boundary[0] != '\0', "boundary generated");
    CHECK(contains(out, "Content-Disposition: form-data; name=\"name\""), "part header");
    CHECK(contains(out, "\r\n\r\nhello\r\n"), "part value");
    CHECK(outlen >= 4 && memcmp(out + outlen - 4, "--\r\n", 4) == 0, "closing boundary");
    free(out);

    /* file upload */
    FILE *fp = fopen("test_upload.txt", "wb");
    CHECK(fp != NULL, "create temp file");
    fwrite("filecontent", 1, 11, fp);
    fclose(fp);

    const char *forms2[] = { "up=@test_upload.txt" };
    out = NULL;
    CHECK(url_multipart_build(forms2, 1, &out, &outlen, boundary, sizeof(boundary)) == 0, "file multipart");
    CHECK(contains(out, "filename=\"test_upload.txt\""), "filename header");
    CHECK(contains(out, "filecontent"), "file content");
    CHECK(contains(out, "Content-Type: text/plain"), "mime guess");
    CHECK(outlen >= 4 && memcmp(out + outlen - 4, "--\r\n", 4) == 0, "closing boundary");
    free(out);
    remove("test_upload.txt");
}

static void test_disposition(void) {
    printf("[url_content_disposition_filename]\n");
    char out[256];
    CHECK(url_content_disposition_filename("attachment; filename=\"x.txt\"", out, sizeof(out)) == 0, "quoted");
    CHECK_STR(out, "x.txt", "quoted name");
    CHECK(url_content_disposition_filename("attachment; filename=plain.bin", out, sizeof(out)) == 0, "plain");
    CHECK_STR(out, "plain.bin", "plain name");
    CHECK(url_content_disposition_filename("inline", out, sizeof(out)) != 0, "none");
}

static void test_remote_name(void) {
    printf("[url_remote_name]\n");
    UrlInfo u;
    char out[512];
    url_parse("http://example.com/download/file.txt", &u);
    url_remote_name(&u, NULL, out, sizeof(out));
    CHECK_STR(out, "file.txt", "last segment");
    url_parse("http://example.com/download/", &u);
    url_remote_name(&u, NULL, out, sizeof(out));
    CHECK_STR(out, "index.html", "empty fallback");
    url_parse("http://example.com/a/b%20c.txt", &u);
    url_remote_name(&u, NULL, out, sizeof(out));
    CHECK_STR(out, "b c.txt", "decoded segment");
    url_remote_name(&u, "attachment; filename=\"a.png\"", out, sizeof(out));
    CHECK_STR(out, "a.png", "disposition wins");
}

static void test_headers(void) {
    printf("[url_header_add / url_header_get]\n");
    HeaderList l;
    memset(&l, 0, sizeof(l));
    url_header_add(&l, "Content-Length", " 5 ");
    CHECK_EQ_INT(l.n, 1, "one header");
    CHECK_STR(l.h[0].name, "content-length", "lowercased");
    CHECK_STR(l.h[0].value, "5", "trimmed value");
    url_header_add(&l, "Content-Length", "10");
    CHECK_EQ_INT(l.n, 1, "dedup keeps one");
    CHECK_STR(url_header_get(&l, "CONTENT-LENGTH"), "10", "case-insensitive get");
    CHECK(url_header_get(&l, "missing") == NULL, "missing returns NULL");
}

static void test_build_request(void) {
    printf("[url_build_request]\n");
    UrlCtx *ctx = url_init();
    UrlInfo u;
    url_parse("http://example.com/api", &u);

    size_t len = 0;
    char *req = url_build_request(ctx, &u, "GET", NULL, NULL, NULL, 0, &len);
    CHECK(contains(req, "GET /api HTTP/1.1\r\n"), "request line");
    CHECK(contains(req, "Host: example.com\r\n"), "host header");
    CHECK(contains(req, "User-Agent: curl/8.6.0\r\n"), "default UA");
    CHECK(contains(req, "Accept: */*\r\n"), "accept header");
    CHECK(contains(req, "Connection: close\r\n"), "connection header");
    CHECK(!contains(req, "content-length"), "no CL for GET");
    free(req);

    char *body = "a=1";
    req = url_build_request(ctx, &u, "POST", "application/x-www-form-urlencoded", NULL, body, 3, &len);
    CHECK(contains(req, "Content-Type: application/x-www-form-urlencoded\r\n"), "content-type");
    CHECK(contains(req, "Content-Length: 3\r\n"), "content-length");
    CHECK(len == strlen(req) && len > 3 && strcmp(req + len - 3, "a=1") == 0, "body appended");
    free(req);

    url_set_user_agent(ctx, "my-agent/1");
    req = url_build_request(ctx, &u, "GET", NULL, "bytes=100-", NULL, 0, &len);
    CHECK(contains(req, "User-Agent: my-agent/1\r\n"), "user override");
    CHECK(!contains(req, "curl/8.6.0"), "override removed default");
    CHECK(contains(req, "Range: bytes=100-\r\n"), "range header");
    free(req);

    url_parse("http://example.com:8080/x", &u);
    req = url_build_request(ctx, &u, "GET", NULL, NULL, NULL, 0, &len);
    CHECK(contains(req, "Host: example.com:8080\r\n"), "host with port");
    free(req);

    url_free(ctx);
}

int main(void) {
    printf("mini-curl.c unit tests\n");
    printf("=====================\n");
    test_context();
    test_url_parse();
    test_url_join();
    test_url_decode();
    test_http_parse();
    test_chunk_parse();
    test_chunk_decode();
    test_parse_options();
    test_choose_method();
    test_url_redirect_method();
    test_multipart();
    test_disposition();
    test_remote_name();
    test_headers();
    test_cookies();
    test_build_request();

    if (g_fail) {
        printf("\n%d test(s) FAILED\n", g_fail);
        return 1;
    }
    printf("\nAll tests passed.\n");
    return 0;
}
