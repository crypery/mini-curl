/*
 * Git: https://github.com/crypery
 * Author: https://crypery.com
 * License: GNU AGPL v3 (Affero GPL)
 */

/*
 * mini-curl.h - public interface of the curl-like HTTP/HTTPS command-line client.
 *
 * Keeps the command-line-specific pieces: the Options structure and the
 * option-parsing helpers. The reusable HTTP/HTTPS machinery lives in url.h /
 * url.c, which this header re-exports.
 */

#ifndef CURL_EX_H
#define CURL_EX_H

#include "url.h"

/* C++ linkage guard: allow this header to be included from C++ code so the
 * parse_options/choose_method functions link against a C-compiled mini-curl.c
 * without name mangling. */
#ifdef __cplusplus
extern "C" {
#endif

// Command-line options collected by parse_options().
typedef struct {
    char method[16];            // -X/--request
    int method_set;
    int head;                   // -I/--head
    int location;               // -L/--location
    int verbose;                // -v/--verbose
    int insecure;               // -k/--insecure
    int silent;                 // -s/--silent
    const char *outfile;        // -o/--output
    int remote_name;            // -O/--remote-name
    int resume;                 // -C/--continue-at given
    long resume_offset;         // -1 = auto (from file size), else explicit
    long connect_timeout_ms;    // --connect-timeout, -1 if unset
    int connect_timeout_set;
    int max_redirs;
    char user_agent[256];   // -A/--user-agent
    HeaderList user_headers;    // -H/--header
    const char *data[URL_MAX_DATA];   // -d/--data
    int data_count;
    const char *forms[URL_MAX_FORMS]; // -F/--form
    int form_count;
} Options;

/* Parse the command line. On success fills `o`, stores the index of the first
 * non-option argument in *url_idx and returns 0. Prints an error and returns
 * -1 on a malformed option. */
int parse_options(int argc, char **argv, Options *o, int *url_idx);

/* Select the HTTP method exactly like curl: -X wins, else HEAD for -I,
 * POST for -F/-d, otherwise GET. */
int choose_method(const Options *o, char *out, size_t outlen);

#ifdef __cplusplus
}
#endif

#endif // CURL_EX_H
