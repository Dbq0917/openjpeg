/*
 * $Id$
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2010-2011, Kaori Hagihara
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "string_compat.h"

#ifdef SERVER
#include "fcgi_stdio.h"
#else
#define FCGI_stderr stderr
#endif /*SERVER*/

#if defined(_WIN32)
#define strnlen opj_strnlen_local
static size_t opj_strnlen_local(const char *s, size_t maxlen)
{
    size_t n = 0;
    while (n < maxlen && s[n] != '\0') {
        n++;
    }
    return n;
}

#if defined(_MSC_VER) && _MSC_VER < 1400
#define vsnprintf _vsnprintf
#endif
#endif

static void opj_report_overflow(const char *field, size_t required,
                                size_t available)
{
    fprintf(FCGI_stderr,
            "Error: field '%s' is too long: %u byte(s) required but only "
            "%u byte(s) available\n",
            field ? field : "(unknown)", (unsigned)required,
            (unsigned)available);
}

OPJ_BOOL opj_strlcpy(char *dst, size_t dst_size, const char *src,
                     const char *field)
{
    size_t src_len;

    if (dst == NULL || src == NULL || dst_size == 0U) {
        opj_report_overflow(field, dst == NULL ? 0U : dst_size, 0U);
        return OPJ_FALSE;
    }

    src_len = strlen(src);
    if (src_len + 1U > dst_size) {
        opj_report_overflow(field, src_len + 1U, dst_size);
        memcpy(dst, src, dst_size - 1U);
        dst[dst_size - 1U] = '\0';
        return OPJ_FALSE;
    }

    memcpy(dst, src, src_len + 1U);
    return OPJ_TRUE;
}

OPJ_BOOL opj_strlcat(char *dst, size_t dst_size, const char *src,
                     const char *field)
{
    size_t dst_len;
    size_t src_len;

    if (dst == NULL || src == NULL || dst_size == 0U) {
        opj_report_overflow(field, dst_size, 0U);
        return OPJ_FALSE;
    }

    /* locate the existing terminating NUL without scanning past the buffer */
    dst_len = strnlen(dst, dst_size);
    if (dst_len == dst_size) {
        opj_report_overflow(field, dst_len + 1U, dst_size);
        return OPJ_FALSE;
    }

    src_len = strlen(src);
    if (dst_len + src_len + 1U > dst_size) {
        opj_report_overflow(field, dst_len + src_len + 1U, dst_size);
        return OPJ_FALSE;
    }

    memcpy(dst + dst_len, src, src_len + 1U);
    return OPJ_TRUE;
}

OPJ_BOOL opj_snprintf(char *dst, size_t dst_size, const char *field,
                      const char *fmt, ...)
{
    int written;
    va_list args;

    if (dst == NULL || dst_size == 0U || fmt == NULL) {
        opj_report_overflow(field, dst_size, 0U);
        return OPJ_FALSE;
    }

    va_start(args, fmt);
    written = vsnprintf(dst, dst_size, fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= dst_size) {
        opj_report_overflow(field, (size_t)(written < 0 ? 0 : written + 1),
                            dst_size);
        dst[dst_size - 1U] = '\0';
        return OPJ_FALSE;
    }

    return OPJ_TRUE;
}

OPJ_BOOL opj_strncpy(char *dst, size_t dst_size, const char *src, size_t n,
                     const char *field)
{
    if (dst == NULL || src == NULL || dst_size == 0U) {
        opj_report_overflow(field, dst_size, 0U);
        return OPJ_FALSE;
    }

    if (n >= dst_size) {
        opj_report_overflow(field, n + 1U, dst_size);
        return OPJ_FALSE;
    }

    memcpy(dst, src, n);
    dst[n] = '\0';
    return OPJ_TRUE;
}
