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

#ifndef     STRING_COMPAT_H_
# define    STRING_COMPAT_H_

#include <stddef.h>
#include "opj_includes.h"

/**
 * Bounded string copy.
 *
 * Copies at most @p dst_size - 1 bytes from @p src to @p dst and always
 * NUL-terminates the destination (when @p dst_size is greater than zero).
 * Unlike strncpy(), it never writes past the destination buffer and never
 * silently truncates: if the source does not fit, the function reports the
 * overflow instead of returning a truncated string.
 *
 * @param[in] dst      destination buffer
 * @param[in] dst_size total size of the destination buffer in bytes
 * @param[in] src      NUL-terminated source string
 * @param[in] field    human readable name of the field being copied, used in
 *                     the error message
 * @return     OPJ_TRUE if the whole source string was copied, OPJ_FALSE if a
 *             NULL argument was given or the source does not fit (the
 *             destination still contains a safe, NUL-terminated prefix)
 */
OPJ_BOOL opj_strlcpy(char *dst, size_t dst_size, const char *src,
                     const char *field);

/**
 * Bounded string concatenation.
 *
 * Appends @p src to the NUL-terminated string already stored in @p dst while
 * guaranteeing that the result fits in @p dst_size bytes, including the
 * terminating NUL. If the concatenation would overflow, the function reports
 * the overflow instead of writing past the buffer or truncating silently.
 *
 * @param[in] dst      destination buffer holding a NUL-terminated string
 * @param[in] dst_size total size of the destination buffer in bytes
 * @param[in] src      NUL-terminated string to append
 * @param[in] field    human readable name of the field being concatenated
 * @return     OPJ_TRUE on success, OPJ_FALSE on NULL argument or overflow
 */
OPJ_BOOL opj_strlcat(char *dst, size_t dst_size, const char *src,
                     const char *field);

/**
 * Bounded formatted output.
 *
 * Wrapper around snprintf() that treats output truncation as an error rather
 * than silently accepting it.
 *
 * @param[in] dst      destination buffer
 * @param[in] dst_size total size of the destination buffer in bytes
 * @param[in] field    human readable name of the field being formatted
 * @param[in] fmt      printf-like format string
 * @param[in] ...      format arguments
 * @return     OPJ_TRUE if the whole formatted output fit, OPJ_FALSE on
 *             truncation or encoding error
 */
OPJ_BOOL opj_snprintf(char *dst, size_t dst_size, const char *field,
                      const char *fmt, ...);

/**
 * Bounded copy of a prefix of a source string.
 *
 * Copies exactly @p n bytes starting at @p src into @p dst and appends a
 * terminating NUL. The destination must therefore hold at least @p n + 1
 * bytes. This replaces strncpy() call sites where the number of bytes to copy
 * comes from attacker controlled input.
 *
 * @param[in] dst      destination buffer
 * @param[in] dst_size total size of the destination buffer in bytes
 * @param[in] src      source bytes (not required to be NUL-terminated)
 * @param[in] n        number of bytes to copy from @p src
 * @param[in] field    human readable name of the field being copied
 * @return     OPJ_TRUE on success, OPJ_FALSE on NULL argument or overflow
 */
OPJ_BOOL opj_strncpy(char *dst, size_t dst_size, const char *src, size_t n,
                     const char *field);

#endif      /* !STRING_COMPAT_H_ */
