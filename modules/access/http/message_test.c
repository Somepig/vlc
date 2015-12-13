/*****************************************************************************
 * message_test.c: HTTP request/response test
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#undef NDEBUG

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include "message.h"
#include "h2frame.h"

static void check_req(const struct vlc_http_msg *m)
{
    const char *str;

    assert(vlc_http_msg_get_status(m) < 0);
    str = vlc_http_msg_get_method(m);
    assert(str != NULL && !strcmp(str, "GET"));
    str = vlc_http_msg_get_scheme(m);
    assert(str != NULL && !strcmp(str, "http"));
    str = vlc_http_msg_get_authority(m);
    assert(str != NULL && !strcmp(str, "www.example.com"));
    str = vlc_http_msg_get_path(m);
    assert(str != NULL && !strcmp(str, "/"));

    str = vlc_http_msg_get_header(m, "Cache-Control");
    assert(str != NULL && !strcmp(str, "no-cache"));
    str = vlc_http_msg_get_header(m, "Custom-Key");
    assert(str != NULL && !strcmp(str, "custom-value"));

    str = vlc_http_msg_get_header(m, "Date");
    assert(str == NULL);
}

static void check_resp(const struct vlc_http_msg *m)
{
    const char *str;

    assert(vlc_http_msg_get_status(m) == 200);
    str = vlc_http_msg_get_method(m);
    assert(str == NULL);
    str = vlc_http_msg_get_scheme(m);
    assert(str == NULL);
    str = vlc_http_msg_get_authority(m);
    assert(str == NULL);
    str = vlc_http_msg_get_path(m);
    assert(str == NULL);

    str = vlc_http_msg_get_header(m, "Cache-Control");
    assert(str != NULL && !strcmp(str, "private"));
    str = vlc_http_msg_get_header(m, "Date");
    assert(str != NULL && !strcmp(str, "Mon, 21 Oct 2013 20:13:22 GMT"));
    str = vlc_http_msg_get_header(m, "Location");
    assert(str != NULL && !strcmp(str, "https://www.example.com"));
    str = vlc_http_msg_get_header(m, "Content-Encoding");
    assert(str != NULL && !strcmp(str, "gzip"));
    str = vlc_http_msg_get_header(m, "Set-Cookie");
    assert(str != NULL && !strcmp(str, "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                                  "max-age=3600; version=1"));

    str = vlc_http_msg_get_header(m, "Custom-Key");
    assert(str == NULL);
}

static void check_connect(const struct vlc_http_msg *m)
{
    const char *str;

    assert(vlc_http_msg_get_status(m) < 0);
    str = vlc_http_msg_get_method(m);
    assert(str != NULL && !strcmp(str, "CONNECT"));
    str = vlc_http_msg_get_scheme(m);
    assert(str == NULL);
    str = vlc_http_msg_get_authority(m);
    assert(str != NULL && !strcmp(str, "www.example.com"));
    str = vlc_http_msg_get_path(m);
    assert(str == NULL);

    str = vlc_http_msg_get_header(m, "Custom-Key");
    assert(str == NULL);
}

static void check_msg(struct vlc_http_msg *in,
                      void (*cb)(const struct vlc_http_msg *))
{
    struct vlc_http_msg *out;
    char *m1;
    size_t len;

    cb(in);

    m1 = vlc_http_msg_format(in, &len);
    assert(m1 != NULL);
    assert(strlen(m1) == len);
    out = vlc_http_msg_headers(m1);
    fprintf(stderr, "%s", m1);
    free(m1);
    /* XXX: request parsing not implemented/needed yet */
    if (vlc_http_msg_get_status(in) >= 0)
    {
        assert(out != NULL);
        cb(out);
        vlc_http_msg_destroy(out);
    }

    out = (struct vlc_http_msg *)vlc_http_msg_h2_frame(in, 1, true);
    assert(out != NULL);
    cb(out);
    assert(vlc_http_msg_read(out) == NULL);
    vlc_http_msg_destroy(out);

    cb(in);
    vlc_http_msg_destroy(in);
}

static time_t parse_date(const char *str)
{
    struct vlc_http_msg *m;
    time_t t1, t2;

    m = vlc_http_req_create("GET", "http", "www.example.com", "/");
    assert(m != NULL);
    assert(vlc_http_msg_add_header(m, "Date", "%s", str) == 0);
    t1 = vlc_http_msg_get_atime(m);
    assert(vlc_http_msg_add_header(m, "Last-Modified", "%s", str) == 0);
    t2 = vlc_http_msg_get_mtime(m);
    assert(vlc_http_msg_add_header(m, "Retry-After", "%s", str) == 0);
    vlc_http_msg_get_retry_after(m);
    vlc_http_msg_destroy(m);

    assert(t1 == t2);
    return t1;
}

int main(void)
{
    struct vlc_http_msg *m;
    int ret;

    /* Formatting and parsing */
    m = vlc_http_req_create("GET", "http", "www.example.com", "/");
    assert(m != NULL);
    ret = vlc_http_msg_add_header(m, "Cache-Control", "no-cache");
    assert(ret == 0);
    vlc_http_msg_add_header(m, "Custom-Key", "%s", "custom-value");
    assert(ret == 0);
    check_msg(m, check_req);

    m = vlc_http_resp_create(200);
    assert(m != NULL);
    ret = vlc_http_msg_add_header(m, "cache-control", "private");
    assert(ret == 0);
    ret = vlc_http_msg_add_header(m, "date", "Mon, 21 Oct 2013 20:13:22 GMT");
    assert(ret == 0);
    ret = vlc_http_msg_add_header(m, "location", "https://www.example.com");
    assert(ret == 0);
    ret = vlc_http_msg_add_header(m, "content-encoding", "gzip");
    assert(ret == 0);
    ret = vlc_http_msg_add_header(m, "set-cookie", "foo=%s; max-age=%u; "
                                  "version=%u", "ASDJKHQKBZXOQWEOPIUAXQWEOIU",
                                  3600, 1);
    assert(ret == 0);
    check_msg(m, check_resp);

    m = vlc_http_req_create("CONNECT", NULL, "www.example.com", NULL);
    assert(m != NULL);
    check_msg(m, check_connect);

    /* Helpers */
    assert(parse_date("Sun, 06 Nov 1994 08:49:37 GMT") == 784111777);
    assert(parse_date("Sunday, 06-Nov-94 08:49:37 GMT") == 784111777);
    assert(parse_date("Sun Nov  6 08:49:37 1994") == 784111777);
    assert(parse_date("Sunday, 06-Nov-14 08:49:37 GMT") == 1415263777);
    assert(parse_date("Sun, 06 Bug 1994 08:49:37 GMT") == -1);
    assert(parse_date("bogus") == -1);

    m = vlc_http_req_create("PRI", "https", "*", NULL);
    assert(m != NULL);

    assert(vlc_http_msg_add_agent(m, "Foo") == 0);
    assert(vlc_http_msg_add_agent(m, "Foo/1.0") == 0);
    assert(vlc_http_msg_add_agent(m, "Foo/1.0 (Hello world) Bar/2.3") == 0);
    assert(vlc_http_msg_add_agent(m, "Foo/1.0 (compatible (\\(!))") == 0);

    assert(vlc_http_msg_add_atime(m) == 0);
    time_t t = vlc_http_msg_get_atime(m);
    assert(t != (time_t)-1);

    vlc_http_msg_add_header(m, "Content-Length", "1234");
    assert(vlc_http_msg_get_size(m) == 1234);

    /* Error cases */
    assert(vlc_http_msg_add_agent(m, "") != 0);
    assert(vlc_http_msg_add_agent(m, "/1.0") != 0);
    assert(vlc_http_msg_add_agent(m, "Bad/1.0\"") != 0);
    assert(vlc_http_msg_add_agent(m, "Bad/1.0 (\\)") != 0);
    assert(vlc_http_msg_add_agent(m, "Bad/1.0 (\\\x08)") != 0);
    assert(vlc_http_msg_add_agent(m, "Bad/1.0 \"Evil\"") != 0);
    assert(vlc_http_msg_add_agent(m, "(Hello world)") != 0);

    vlc_http_msg_destroy(m);

    char *bad1[][2] = {
        { strdup(":status"), strdup("200") },
        { strdup(":status"), strdup("200") },
        { strdup("Server"),  strdup("BigBad/1.0") },
    };

    m = vlc_http_msg_h2_headers(3, bad1);
    assert(m == NULL);

    return 0;
}

/* Callback for vlc_http_msg_h2_frame */
struct vlc_h2_frame *
vlc_h2_frame_headers(uint_fast32_t id, uint_fast32_t mtu, bool eos,
                     unsigned count, const char *const tab[][2])
{
    struct vlc_http_msg *m;

    assert(id == 1);
    assert(mtu == VLC_H2_DEFAULT_MAX_FRAME);
    assert(eos);

    char *headers[VLC_H2_MAX_HEADERS][2];

    for (unsigned i = 0; i < count; i++)
    {
        headers[i][0] = strdup(tab[i][0]);
        headers[i][1] = strdup(tab[i][1]);
    }

    m = vlc_http_msg_h2_headers(count, headers);
    return (struct vlc_h2_frame *)m; /* gruik */
}