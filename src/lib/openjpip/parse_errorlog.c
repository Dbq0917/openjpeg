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
#include "parse_errorlog.h"

void parse_error_stats_init(parse_error_stats_t *stats)
{
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
}

void parse_error_stats_record(parse_error_stats_t *stats,
                              const query_error_t *error)
{
    query_error_type_t type;

    if (stats == NULL || error == NULL) {
        return;
    }

    type = error->type;
    if (type < QERR_NONE || type >= QUERY_ERROR_TYPE_COUNT) {
        type = QERR_MALFORMED;
    }
    stats->count[type]++;
}

void parse_error_stats_report(const parse_error_stats_t *stats, FILE *fp)
{
    int i;
    unsigned long long total = 0;

    if (stats == NULL || fp == NULL) {
        return;
    }

    for (i = 0; i < QUERY_ERROR_TYPE_COUNT; i++) {
        total += stats->count[i];
    }

    fprintf(fp, "parse error statistics (%llu record(s))\n", total);
    for (i = 0; i < QUERY_ERROR_TYPE_COUNT; i++) {
        fprintf(fp, "%-22s %llu\n", query_error_type_name[i],
                stats->count[i]);
    }
}

OPJ_BOOL parse_error_stats_save(const parse_error_stats_t *stats,
                                const char *filepath)
{
    FILE *fp;
    int i;

    if (stats == NULL || filepath == NULL) {
        return OPJ_FALSE;
    }

    fp = fopen(filepath, "wb");
    if (fp == NULL) {
        return OPJ_FALSE;
    }

    for (i = 0; i < QUERY_ERROR_TYPE_COUNT; i++) {
        fprintf(fp, "%s=%llu\n", query_error_type_name[i], stats->count[i]);
    }

    fclose(fp);
    return OPJ_TRUE;
}

OPJ_BOOL parse_error_stats_load(parse_error_stats_t *stats,
                                const char *filepath)
{
    FILE *fp;
    char key[64];
    unsigned long long value;
    int i;

    if (stats == NULL || filepath == NULL) {
        return OPJ_FALSE;
    }

    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        return OPJ_FALSE;
    }

    while (fscanf(fp, "%63[^=]=%llu\n", key, &value) == 2) {
        for (i = 0; i < QUERY_ERROR_TYPE_COUNT; i++) {
            if (strcmp(key, query_error_type_name[i]) == 0) {
                stats->count[i] += value;
                break;
            }
        }
    }

    fclose(fp);
    return OPJ_TRUE;
}
