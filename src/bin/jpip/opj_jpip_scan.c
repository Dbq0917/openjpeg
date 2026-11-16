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

/*! \file
 *  \brief batch re-parser for archived JPIP query strings
 *
 *  Re-parses every archived query string (one per line) and reports length
 *  violations grouped by error type, so that records which could have been
 *  corrupted by the historical unbounded string copies can be identified and
 *  re-processed. Designed to run over the full archive (millions of records):
 *  it streams input, keeps fixed-size counters and never aborts on one bad
 *  record.
 *
 *  Usage:
 *    opj_jpip_scan [-f manifest] [-d directory] [-s stats-file]
 *                  [-o suspect-file] [--stdin]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* this is an application, not library code: use libc allocation directly */
#define OPJ_SKIP_POISON

#include "query_parser.h"
#include "parse_errorlog.h"

#define MAX_QUERY_LINE 65536

typedef struct scan_context {
    parse_error_stats_t stats;
    unsigned long long scanned;
    FILE *suspect_fp;
} scan_context_t;

static void scan_one_line(scan_context_t *ctx, const char *source,
                          const char *line)
{
    query_error_t error;
    query_param_t *query;

    ctx->scanned++;
    query = parse_query_ex(line, &error);
    parse_error_stats_record(&ctx->stats, &error);

    if (error.type != QERR_NONE && ctx->suspect_fp != NULL) {
        fprintf(ctx->suspect_fp, "%s\t%s\tfield=%s\trequired=%zu\tcapacity=%zu\n",
                query_error_type_name[error.type],
                source ? source : "-",
                error.field[0] ? error.field : "(unknown)",
                error.required, error.capacity);
    }

    delete_query(&query);
}

static int scan_stream(scan_context_t *ctx, const char *source, FILE *fp)
{
    char *line = (char *)malloc(MAX_QUERY_LINE);

    if (line == NULL) {
        return -1;
    }

    while (fgets(line, MAX_QUERY_LINE, fp) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        if (len == MAX_QUERY_LINE - 1U) {
            /* an overlong physical line: drain the remainder and count it as
             * a malformed record rather than silently scanning a prefix */
            int ch;
            while ((ch = fgetc(fp)) != EOF && ch != '\n') {
            }
            ctx->scanned++;
            ctx->stats.count[QERR_MALFORMED]++;
            if (ctx->suspect_fp != NULL) {
                fprintf(ctx->suspect_fp,
                        "%s\t%s\tfield=(line)\trequired=>%d\tcapacity=%d\n",
                        query_error_type_name[QERR_MALFORMED],
                        source ? source : "-", MAX_QUERY_LINE,
                        MAX_QUERY_LINE);
            }
            continue;
        }
        scan_one_line(ctx, source, line);
    }

    free(line);
    return 0;
}

static int scan_file(scan_context_t *ctx, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open manifest '%s'\n", path);
        return -1;
    }
    if (scan_stream(ctx, path, fp) != 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int scan_directory(scan_context_t *ctx, const char *dirpath)
{
    DIR *dir = opendir(dirpath);
    struct dirent *entry;

    if (dir == NULL) {
        fprintf(stderr, "Error: cannot open directory '%s'\n", dirpath);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[4096];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(path, sizeof(path), "%s/%s", dirpath,
                     entry->d_name) >= (int)sizeof(path)) {
            fprintf(stderr, "Error: path too long, skipping '%s'\n",
                    entry->d_name);
            continue;
        }
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        if (scan_file(ctx, path) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    return 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-f manifest-file] [-d directory] [--stdin]\n"
            "                  [-s stats-output] [-o suspect-output]\n",
            prog);
}

int main(int argc, char *argv[])
{
    scan_context_t ctx;
    const char *manifest = NULL;
    const char *directory = NULL;
    const char *stats_path = NULL;
    const char *suspect_path = NULL;
    int use_stdin = 0;
    int i;

    parse_error_stats_init(&ctx.stats);
    ctx.scanned = 0;
    ctx.suspect_fp = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            manifest = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            directory = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            stats_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            suspect_path = argv[++i];
        } else if (strcmp(argv[i], "--stdin") == 0) {
            use_stdin = 1;
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (manifest == NULL && directory == NULL && !use_stdin) {
        print_usage(argv[0]);
        return 2;
    }

    if (suspect_path != NULL) {
        ctx.suspect_fp = fopen(suspect_path, "wb");
        if (ctx.suspect_fp == NULL) {
            fprintf(stderr, "Error: cannot open suspect file '%s'\n",
                    suspect_path);
            return 1;
        }
    }

    if (manifest != NULL && scan_file(&ctx, manifest) != 0) {
        return 1;
    }
    if (directory != NULL && scan_directory(&ctx, directory) != 0) {
        return 1;
    }
    if (use_stdin && scan_stream(&ctx, "(stdin)", stdin) != 0) {
        return 1;
    }

    fprintf(stdout, "scanned %llu record(s)\n", ctx.scanned);
    parse_error_stats_report(&ctx.stats, stdout);

    if (stats_path != NULL &&
            !parse_error_stats_save(&ctx.stats, stats_path)) {
        fprintf(stderr, "Error: failed to write stats file '%s'\n",
                stats_path);
        return 1;
    }

    if (ctx.suspect_fp != NULL) {
        fclose(ctx.suspect_fp);
    }

    /* exit non-zero when any record failed to parse, useful in CI/cron */
    for (i = (int)QERR_MALFORMED; i < QUERY_ERROR_TYPE_COUNT; i++) {
        if (ctx.stats.count[i] != 0U) {
            return 3;
        }
    }
    return 0;
}
