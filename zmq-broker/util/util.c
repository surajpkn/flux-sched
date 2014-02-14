#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <json/json.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <math.h>
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <uuid/uuid.h>
#include <assert.h>

#include "util.h"
#include "log.h"

static struct timespec ts_diff (struct timespec start, struct timespec end)
{
        struct timespec temp;
        if ((end.tv_nsec-start.tv_nsec)<0) {
                temp.tv_sec = end.tv_sec-start.tv_sec-1;
                temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
        } else {
                temp.tv_sec = end.tv_sec-start.tv_sec;
                temp.tv_nsec = end.tv_nsec-start.tv_nsec;
        }
        return temp;
}

double monotime_since (struct timespec t0)
{
    struct timespec ts, d;
    clock_gettime (CLOCK_MONOTONIC, &ts);

    d = ts_diff (t0, ts);

    return ((double) d.tv_sec * 1000 + (double) d.tv_nsec / 1000000);
}

void monotime (struct timespec *tp)
{
    clock_gettime (CLOCK_MONOTONIC, tp);
}

bool monotime_isset (struct timespec t)
{
    return (t.tv_sec || t.tv_nsec);
}

void *xzmalloc (size_t size)
{
    void *new;

    new = malloc (size);
    if (!new)
        oom ();
    memset (new, 0, size);
    return new;
}

char *xstrdup (const char *s)
{
    char *cpy = strdup (s);
    if (!cpy)
        oom ();
    return cpy;
}

int setenvf (const char *name, int overwrite, const char *fmt, ...)
{
    va_list ap;
    char *val;
    int rc;

    va_start (ap, fmt);
    rc = vasprintf (&val, fmt, ap);
    va_end (ap);
    if (rc < 0)
        return (rc);

    rc = setenv (name, val, overwrite);
    free (val);
    return (rc);
}

int env_getint (char *name, int dflt)
{
    char *ev = getenv (name);
    return ev ? strtoul (ev, NULL, 10) : dflt;
}

char *env_getstr (char *name, char *dflt)
{
    char *ev = getenv (name);
    return ev ? xstrdup (ev) : xstrdup (dflt);
}

static int _strtoia (char *s, int *ia, int ia_len)
{
    char *next;
    int n, len = 0;

    while (*s) {
        n = strtoul (s, &next, 10);
        s = *next == '\0' ? next : next + 1;
        if (ia) {
            if (ia_len == len)
                break;
            ia[len] = n;
        }
        len++;
    }
    return len;
}

int getints (char *s, int **iap, int *lenp)
{
    int len = _strtoia (s, NULL, 0);
    int *ia = malloc (len * sizeof (int));

    if (!ia)
        return -1;

    (void)_strtoia (s, ia, len);
    *lenp = len;
    *iap = ia;
    return 0;
}

int env_getints (char *name, int **iap, int *lenp, int dflt_ia[], int dflt_len)
{
    char *s = getenv (name);
    int *ia;
    int len;

    if (s) {
        if (getints (s, &ia, &len) < 0)
            return -1;
    } else {
        ia = malloc (dflt_len * sizeof (int));
        if (!ia)
            return -1;
        for (len = 0; len < dflt_len; len++)
            ia[len] = dflt_ia[len];
    }
    *lenp = len;
    *iap = ia;
    return 0;
}

char *argv_concat (int argc, char *argv[])
{
    int i, len = 0;
    char *s;

    for (i = 0; i < argc; i++)
        len += strlen (argv[i]) + 1;
    s = xzmalloc (len + 1);
    for (i = 0; i < argc; i++) {
        strcat (s, argv[i]);
        if (i < argc - 1)
            strcat (s, " "); 
    }
    return s; 
}

char *uuid_generate_str (void)
{
    char s[sizeof (uuid_t) * 2 + 1];
    uuid_t uuid;
    int i;

    uuid_generate (uuid);
    for (i = 0; i < sizeof (uuid_t); i++)
        snprintf (s + i*2, sizeof (s) - i*2, "%-.2x", uuid[i]);
    return xstrdup (s);
}

static void compute_href (const void *dat, int len, href_t href)
{
    unsigned char raw[SHA_DIGEST_LENGTH];
    SHA_CTX ctx;
    int i;

    assert(SHA_DIGEST_LENGTH*2 + 1 == sizeof (href_t));

    if (SHA1_Init (&ctx) == 0)
        err_exit ("%s: SHA1_Init", __FUNCTION__);
    if (SHA1_Update (&ctx, dat, len) == 0)
        err_exit ("%s: SHA1_Update", __FUNCTION__);
    if (SHA1_Final (raw, &ctx) == 0)
        err_exit ("%s: SHA1_Final", __FUNCTION__);
    for (i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf (&href[i*2], "%02x", (unsigned int)raw[i]);
}

int util_json_size (json_object *o)
{
    const char *s = json_object_to_json_string (o);
    return strlen (s);
}

bool util_json_match (json_object *o1, json_object *o2)
{
    const char *s1 = json_object_to_json_string (o1);
    const char *s2 = json_object_to_json_string (o2);

    return !strcmp (s1, s2);
}

void util_json_encode (json_object *o, char **zbufp, unsigned int *zlenp)
{
    const char *s = json_object_to_json_string (o);
    unsigned int s_len = strlen (s);

    *zbufp = xstrdup (s);
    *zlenp = s_len;
}

void util_json_decode (json_object **op, char *zbuf, unsigned int zlen)
{
    json_object *o;
    struct json_tokener *tok;
    char *s;
    int s_len;

    s_len = zlen;
    s = zbuf; 

    if (!(tok = json_tokener_new ()))
        oom ();
    o = json_tokener_parse_ex (tok, s, s_len);
    json_tokener_free (tok); 
    *op = o;
}

void compute_json_href (json_object *o, href_t href)
{
    const char *s = json_object_to_json_string (o);

    compute_href (s, strlen (s), href);
}

void util_json_object_add_boolean (json_object *o, char *name, bool val)
{
    json_object *no;

    if (!(no = json_object_new_boolean (val)))
        oom ();
    json_object_object_add (o, name, no);
}

void util_json_object_add_double (json_object *o, char *name, double n)
{
    json_object *no;

    if (!(no = json_object_new_double (n)))
        oom ();
    json_object_object_add (o, name, no);
}

void util_json_object_add_int (json_object *o, char *name, int i)
{
    json_object *no;

    if (!(no = json_object_new_int (i)))
        oom ();
    json_object_object_add (o, name, no);
}

void util_json_object_add_int64 (json_object *o, char *name, int64_t i)
{
    json_object *no;

    if (!(no = json_object_new_int64 (i)))
        oom ();
    json_object_object_add (o, name, no);
}


void util_json_object_add_string (json_object *o, char *name, const char *s)
{
    json_object *no;

    if (!(no = json_object_new_string (s)))
        oom ();
    json_object_object_add (o, name, no);
}

void util_json_object_add_base64 (json_object *o, char *name,
                                  uint8_t *dat, int len)
{
    EVP_ENCODE_CTX ectx;
    size_t size = len*2;
    uint8_t *out;
    int outlen = 0;
    int tlen = 0;

    if (size < 64)
        size = 64;
    out = xzmalloc (size + 1);
    EVP_EncodeInit (&ectx);
    EVP_EncodeUpdate (&ectx, out, &outlen, dat, len);
    tlen += outlen;
    EVP_EncodeFinal (&ectx, out + tlen, &outlen);
    tlen += outlen;
    assert (tlen < size);
    if (out[tlen - 1] == '\n')
        out[tlen - 1] = '\0';
    util_json_object_add_string (o, name, (char *)out);
    free (out);
}

void util_json_object_add_timeval (json_object *o, char *name,
                                   struct timeval *tvp)
{
    json_object *no;
    char tbuf[32];

    snprintf (tbuf, sizeof (tbuf), "%lu.%lu", tvp->tv_sec, tvp->tv_usec);
    if (!(no = json_object_new_string (tbuf)))
        oom ();
    json_object_object_add (o, name, no);
}

int util_json_object_get_boolean (json_object *o, char *name, bool *vp)
{
    json_object *no = json_object_object_get (o, name);
    if (!no)
        return -1;
    *vp = json_object_get_boolean (no);
    return 0;
}

int util_json_object_get_double (json_object *o, char *name, double *dp)
{
    json_object *no = json_object_object_get (o, name);
    if (!no)
        return -1;
    *dp = json_object_get_double (no);
    return 0;
}

int util_json_object_get_int (json_object *o, char *name, int *ip)
{
    json_object *no = json_object_object_get (o, name);
    if (!no)
        return -1;
    *ip = json_object_get_int (no);
    return 0;
}

int util_json_object_get_int64 (json_object *o, char *name, int64_t *ip)
{
    json_object *no = json_object_object_get (o, name);
    if (!no)
        return -1;
    *ip = json_object_get_int64 (no);
    return 0;
}

int util_json_object_get_string (json_object *o, char *name, const char **sp)
{
    json_object *no = json_object_object_get (o, name);
    if (!no)
        return -1;
    *sp = json_object_get_string (no);
    return 0;
}

int util_json_object_get_base64 (json_object *o, char *name,
                                 uint8_t **datp, int *lenp)
{
    const char *s;
    int slen;
    EVP_ENCODE_CTX ectx;
    uint8_t *out = NULL;
    int outlen = 0;
    int tlen = 0;

    if (util_json_object_get_string (o, name, &s) == 0) {
        slen = strlen (s);
        out = xzmalloc (slen);
        EVP_DecodeInit (&ectx);
        EVP_DecodeUpdate (&ectx, out, &outlen, (uint8_t *)s, slen);
        tlen += outlen;
        EVP_DecodeFinal (&ectx, out + tlen, &outlen);
        tlen += outlen;
    }
    *datp = out;
    *lenp = tlen;    
    return 0;
}

int util_json_object_get_timeval (json_object *o, char *name,
                                  struct timeval *tvp)
{
    struct timeval tv;
    char *endptr;
    json_object *no = json_object_object_get (o, name);
    if (!no)
        return -1;
    tv.tv_sec = strtoul (json_object_get_string (no), &endptr, 10);
    tv.tv_usec = *endptr ? strtoul (endptr + 1, NULL, 10) : 0;
    *tvp = tv;
    return 0;
}

int util_json_object_get_int_array (json_object *o, char *name,
                                    int **ap, int *lp)
{
    json_object *no = json_object_object_get (o, name);
    json_object *vo;
    int i, len, *arr = NULL;

    if (!no)
        goto error;
    len = json_object_array_length (no);
    arr = xzmalloc (sizeof (int) * len);
    for (i = 0; i < len; i++) {
        vo = json_object_array_get_idx (no, i);
        if (!vo)
            goto error;
        arr[i] = json_object_get_int (vo);
    }
    *ap = arr;
    *lp = len;
    return 0;
error:
    if (arr)
        free (arr);
    return -1;
}

json_object *util_json_object_new_object (void)
{
    json_object *o;

    if (!(o = json_object_new_object ()))
        oom ();
    return o;
}

json_object *rusage_to_json (struct rusage *ru)
{
    json_object *o = util_json_object_new_object ();
    util_json_object_add_timeval (o, "utime", &ru->ru_utime);
    util_json_object_add_timeval (o, "stime", &ru->ru_stime);
    util_json_object_add_int64 (o, "maxrss", ru->ru_maxrss);
    util_json_object_add_int64 (o, "ixrss", ru->ru_ixrss);
    util_json_object_add_int64 (o, "idrss", ru->ru_idrss);
    util_json_object_add_int64 (o, "isrss", ru->ru_isrss);
    util_json_object_add_int64 (o, "minflt", ru->ru_minflt);
    util_json_object_add_int64 (o, "majflt", ru->ru_majflt);
    util_json_object_add_int64 (o, "nswap", ru->ru_nswap);
    util_json_object_add_int64 (o, "inblock", ru->ru_inblock);
    util_json_object_add_int64 (o, "oublock", ru->ru_oublock);
    util_json_object_add_int64 (o, "msgsnd", ru->ru_msgsnd);
    util_json_object_add_int64 (o, "msgrcv", ru->ru_msgrcv);
    util_json_object_add_int64 (o, "nsignals", ru->ru_nsignals);
    util_json_object_add_int64 (o, "nvcsw", ru->ru_nvcsw);
    util_json_object_add_int64 (o, "nivcsw", ru->ru_nivcsw);
    return o;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
