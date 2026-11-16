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

#ifndef     PARSE_ERRORLOG_H_
# define    PARSE_ERRORLOG_H_

#include <stdio.h>
#include "query_parser.h"

/**
 * Aggregated parse error counters, one per query_error_type_t category.
 * Index 0 (QERR_NONE) counts the successfully parsed records.
 */
typedef struct parse_error_stats {
    unsigned long long count[QUERY_ERROR_TYPE_COUNT];
} parse_error_stats_t;

/**
 * Initialize all counters (including the success counter) to zero.
 */
void parse_error_stats_init(parse_error_stats_t *stats);

/**
 * Record one parse outcome.
 *
 * @param[in] stats counters
 * @param[in] error parse error returned by parse_query_ex()
 */
void parse_error_stats_record(parse_error_stats_t *stats,
                              const query_error_t *error);

/**
 * Write a human readable, line-oriented report grouped by error type.
 *
 * @param[in] stats counters
 * @param[in] fp    output stream (e.g. stdout or a report file)
 */
void parse_error_stats_report(const parse_error_stats_t *stats, FILE *fp);

/**
 * Persist the counters as key=value lines so that a full archive scan can be
 * resumed or merged later.
 *
 * @param[in] stats    counters
 * @param[in] filepath destination file path
 * @return    OPJ_TRUE on success, OPJ_FALSE if the file could not be written
 */
OPJ_BOOL parse_error_stats_save(const parse_error_stats_t *stats,
                                const char *filepath);

/**
 * Merge counters previously written by parse_error_stats_save() into @p stats.
 * Unknown keys are ignored so older or newer report files stay compatible.
 *
 * @param[in] stats    counters to add to
 * @param[in] filepath source report file path
 * @return    OPJ_TRUE on success, OPJ_FALSE if the file could not be read
 */
OPJ_BOOL parse_error_stats_load(parse_error_stats_t *stats,
                                const char *filepath);

#endif      /* !PARSE_ERRORLOG_H_ */
