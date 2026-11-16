/*
 * $Id$
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2010-2011, Kaori Hagihara
 * Copyright (c) 2011,      Lucian Corlaciu, GSoC
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


#ifdef _WIN32
#include <windows.h>
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "query_parser.h"

#ifdef SERVER
#include "fcgi_stdio.h"
#define logstream FCGI_stdout
#else
#define FCGI_stdout stdout
#define FCGI_stderr stderr
#define logstream stderr
#endif /*SERVER*/


/**
 * Get initialized query parameters
 *
 * @return initial query parameters
 */
query_param_t * get_initquery(void);

OPJ_BOOL parse_cclose(char *src, query_param_t *query_param);
OPJ_BOOL parse_metareq(char *field, query_param_t *query_param);

/* parse the requested components (parses forms like:a; a,b; a-b; a-b,c;  a,b-c)*/
OPJ_BOOL parse_comps(char *field, query_param_t *query_param);


/** maximum length of field name*/
#define MAX_LENOFFIELDNAME 10

/** maximum length of field value*/
#define MAX_LENOFFIELDVAL 128

/** parse result: request syntax is not well formed */
#define QUERY_PARSE_FORMAT_ERROR 0
/** parse result: a field name/value is syntactically valid but exceeds the
 * allowed length (it must be rejected before any bounded buffer is written) */
#define QUERY_PARSE_LENGTH_ERROR 1
/** parse result: request parsed successfully */
#define QUERY_PARSE_OK 2

/* Bounded field extraction: validates lengths against the remaining buffer
 * capacity before copying, so an overlong field is rejected rather than
 * overflowing the caller's stack buffers. */
static int get_fieldparam_safe(const char *stringptr, char *fieldname,
                               size_t fieldname_sz, char *fieldval,
                               size_t fieldval_sz, char **nextptr);

query_param_t * parse_query(const char *query_string)
{
    query_param_t *query_param;
    const char *pquery;
    char fieldname[MAX_LENOFFIELDNAME + 1], fieldval[MAX_LENOFFIELDVAL + 1];
    int parse_result;

    query_param = get_initquery();

    pquery = query_string;

    /* An empty query string is valid and simply yields the default,
     * initialized parameter set (this matches the historical behavior). */
    if (pquery == NULL || pquery[0] == '\0') {
        return query_param;
    }

    while (pquery != NULL) {

        {
            char *next_field = NULL;
            parse_result = get_fieldparam_safe(pquery, fieldname,
                                                sizeof(fieldname), fieldval,
                                                sizeof(fieldval), &next_field);
            pquery = next_field;
        }

        if (parse_result != QUERY_PARSE_OK) {
            if (parse_result == QUERY_PARSE_FORMAT_ERROR) {
                fprintf(stderr,
                        "Malformed JPIP query: field without '=' separator\n");
            } else {
                fprintf(stderr,
                        "JPIP query rejected: field name or value exceeds "
                        "maximum length (name <= %d, value <= %d)\n",
                        MAX_LENOFFIELDNAME, MAX_LENOFFIELDVAL);
            }
            delete_query(&query_param);
            return NULL;
        }

        if (fieldname[0] != '\0') {
            if (strcasecmp(fieldname, "target") == 0) {
                query_param->target = strdup(fieldval);
            }

            else if (strcasecmp(fieldname, "tid") == 0) {
                query_param->tid = strdup(fieldval);
            }

            else if (strcasecmp(fieldname, "fsiz") == 0) {
                sscanf(fieldval, "%d,%d", &query_param->fx, &query_param->fy);
            }

            else if (strcasecmp(fieldname, "roff") == 0) {
                sscanf(fieldval, "%d,%d", &query_param->rx, &query_param->ry);
            }

            else if (strcasecmp(fieldname, "rsiz") == 0) {
                sscanf(fieldval, "%d,%d", &query_param->rw, &query_param->rh);
            }

            else if (strcasecmp(fieldname, "layers") == 0) {
                sscanf(fieldval, "%d", &query_param->layers);
            }

            else if (strcasecmp(fieldname, "cid") == 0) {
                query_param->cid = strdup(fieldval);
            }

            else if (strcasecmp(fieldname, "cnew") == 0) {
                if (strncasecmp(fieldval, "http-tcp", 8) == 0) {
                    query_param->cnew = tcp;
                } else if (strncasecmp(fieldval, "http", 4) == 0) {
                    query_param->cnew = http;
                }
            }

            else if (strcasecmp(fieldname, "cclose") == 0) {
                if (! parse_cclose(fieldval, query_param)) {
                    delete_query(&query_param);
                    return NULL;
                }
            }

            else if (strcasecmp(fieldname, "metareq") == 0) {
                if (! parse_metareq(fieldval, query_param)) {
                    delete_query(&query_param);
                    return NULL;
                }
            }

            else if (strcasecmp(fieldname, "comps") == 0) {
                if (! parse_comps(fieldval, query_param)) {
                    delete_query(&query_param);
                    return NULL;
                }
            }

            else if (strcasecmp(fieldname, "type") == 0) {
                if (strncasecmp(fieldval, "jpp-stream", 10) == 0) {
                    query_param->return_type = JPPstream;
                } else if (strncasecmp(fieldval, "jpt-stream", 10) == 0) {
                    query_param->return_type = JPTstream;
                }
            }

            else if (strcasecmp(fieldname, "len") == 0) {
                sscanf(fieldval, "%d", &query_param->len);
                if (query_param->len == 2000) { /* for kakadu client*/
                    memcpy(query_param->box_type[0], "ftyp", 4);
                }
            }
        }
    }
    return query_param;
}

query_param_t * get_initquery(void)
{
    query_param_t *query;
    int i;

    query = (query_param_t *)opj_malloc(sizeof(query_param_t));

    query->target = NULL;
    query->tid = NULL;
    query->fx = -1;
    query->fy = -1;
    query->rx = -1;
    query->ry = -1;
    query->rw = -1;
    query->rh = -1;
    query->layers = -1;
    query->lastcomp = -1;
    query->comps = NULL;
    query->cid = NULL;
    query->cnew = non;
    query->cclose = NULL;
    query->numOfcclose = 0;
    memset(query->box_type, 0, MAX_NUMOFBOX * 4);
    memset(query->limit, 0, MAX_NUMOFBOX * sizeof(int));
    for (i = 0; i < MAX_NUMOFBOX; i++) {
        query->w[i] = OPJ_FALSE;
        query->s[i] = OPJ_FALSE;
        query->g[i] = OPJ_FALSE;
        query->a[i] = OPJ_FALSE;
        query->priority[i] = OPJ_FALSE;
    }
    query->root_bin = 0;
    query->max_depth = -1;
    query->metadata_only = OPJ_FALSE;
    query->return_type = UNKNOWN;
    query->len = -1;

    return query;
}


/**
 * get a pair of field name and value from the string starting
 * fieldname=fieldval&... format
 *
 * Lengths are validated against the actual remaining capacity of the caller
 * buffers BEFORE any byte is copied, so an overlong field can never overflow
 * the stack buffers.
 *
 * @param[in]  stringptr     pointer to the beginning of the parsing string
 * @param[out] fieldname     buffer to copy the field name
 * @param[in]  fieldname_sz  size of the fieldname buffer (including the
 *                           trailing NUL)
 * @param[out] fieldval      buffer to copy the field value
 * @param[in]  fieldval_sz   size of the fieldval buffer (including the
 *                           trailing NUL)
 * @param[out] nextptr       pointer to the next field string, NULL if there is
 *                           none
 * @return QUERY_PARSE_OK on success, QUERY_PARSE_FORMAT_ERROR if the field is
 *         not well formed, QUERY_PARSE_LENGTH_ERROR if a field exceeds its
 *         buffer capacity
 */
static int get_fieldparam_safe(const char *stringptr, char *fieldname,
                               size_t fieldname_sz, char *fieldval,
                               size_t fieldval_sz, char **nextptr)
{
    const char *eqp, *andp;
    size_t name_len, val_len;

    *nextptr = NULL;

    if ((eqp = strchr(stringptr, '=')) == NULL) {
        fprintf(stderr, "= not found\n");
        return QUERY_PARSE_FORMAT_ERROR;
    }
    if ((andp = strchr(stringptr, '&')) == NULL) {
        andp = strchr(stringptr, '\0');
    } else {
        *nextptr = (char *)(andp + 1);
    }

    /* A '&' located before the '=' belongs to a previous malformed field:
     * there is no valid name/value pair in this segment. */
    if (andp < eqp) {
        return QUERY_PARSE_FORMAT_ERROR;
    }

    name_len = (size_t)(eqp - stringptr);
    val_len = (size_t)(andp - eqp - 1);

    if (name_len == 0) {
        return QUERY_PARSE_FORMAT_ERROR;
    }

    /* Validate against the remaining buffer capacity before copying. */
    if (name_len >= fieldname_sz || val_len >= fieldval_sz) {
        return QUERY_PARSE_LENGTH_ERROR;
    }

    memcpy(fieldname, stringptr, name_len);
    fieldname[name_len] = '\0';
    memcpy(fieldval, eqp + 1, val_len);
    fieldval[val_len] = '\0';

    return QUERY_PARSE_OK;
}

void print_queryparam(query_param_t query_param)
{
    int i;
    char *cclose;

    fprintf(logstream, "query parameters:\n");

    if (query_param.target) {
        fprintf(logstream, "\t target: %s\n", query_param.target);
    }

    if (query_param.tid) {
        fprintf(logstream, "\t tid: %s\n", query_param.tid);
    }

    fprintf(logstream, "\t fx,fy: %d, %d\n", query_param.fx, query_param.fy);
    fprintf(logstream, "\t rx,ry: %d, %d \t rw,rh: %d, %d\n", query_param.rx,
            query_param.ry, query_param.rw, query_param.rh);
    fprintf(logstream, "\t layers: %d\n", query_param.layers);
    fprintf(logstream, "\t components: ");

    if (query_param.lastcomp == -1) {
        fprintf(logstream, "ALL\n");
    } else {
        for (i = 0; i <= query_param.lastcomp; i++)
            if (query_param.comps[i]) {
                fprintf(logstream, "%d ", i);
            }
        fprintf(logstream, "\n");
    }
    fprintf(logstream, "\t cnew: %d\n", query_param.cnew);

    if (query_param.cid) {
        fprintf(logstream, "\t cid: %s\n", query_param.cid);
    }

    if (query_param.cclose) {
        fprintf(logstream, "\t cclose: ");

        for (i = 0, cclose = query_param.cclose; i < query_param.numOfcclose; i++) {
            fprintf(logstream, "%s ", cclose);
            cclose += (strlen(cclose) + 1);
        }
        fprintf(logstream, "\n");
    }

    fprintf(logstream, "\t req-box-prop\n");
    for (i = 0; i < MAX_NUMOFBOX && query_param.box_type[i][0] != 0; i++) {
        fprintf(logstream,
                "\t\t box_type: %.4s limit: %d w:%d s:%d g:%d a:%d priority:%d\n",
                query_param.box_type[i], query_param.limit[i], query_param.w[i],
                query_param.s[i], query_param.g[i], query_param.a[i], query_param.priority[i]);
    }

    fprintf(logstream, "\t root-bin:  %d\n", query_param.root_bin);
    fprintf(logstream, "\t max-depth: %d\n", query_param.max_depth);
    fprintf(logstream, "\t metadata-only: %d\n", query_param.metadata_only);
    fprintf(logstream,
            "\t image return type: %d, [JPP-stream=0, JPT-stream=1, UNKNOWN=-1]\n",
            query_param.return_type);
    fprintf(logstream, "\t len:  %d\n", query_param.len);
}

OPJ_BOOL parse_cclose(char *src, query_param_t *query_param)
{
    size_t i;
    size_t len;

    if (src == NULL || src[0] == '\0') {
        fprintf(stderr, "Malformed JPIP query: empty cclose field\n");
        return OPJ_FALSE;
    }

    len = strlen(src);
    query_param->cclose = strdup(src);
    if (query_param->cclose == NULL) {
        fprintf(stderr, "Memory allocation failed while parsing cclose\n");
        return OPJ_FALSE;
    }

    for (i = 0; i < len; i++)
        if (query_param->cclose[i] == ',') {
            query_param->cclose[i] = '\0';
            query_param->numOfcclose ++;
        }

    query_param->numOfcclose ++;
    return OPJ_TRUE;
}

void parse_req_box_prop(char *req_box_prop, int idx,
                        query_param_t *query_param);

/** length of the temporary per-property token in a metareq field */
#define MAX_LENOFREOBOXPROP 19

OPJ_BOOL parse_metareq(char *field, query_param_t *query_param)
{
    char req_box_prop[MAX_LENOFREOBOXPROP + 1];
    char *ptr, *src;
    int numofboxreq = 0;

    memset(req_box_prop, 0, sizeof(req_box_prop));

    /* req-box-prop*/
    ptr = strchr(field, '[');
    if (ptr == NULL) {
        fprintf(stderr,
                "Malformed JPIP query: metareq field missing '[' marker\n");
        return OPJ_FALSE;
    }
    ptr++;
    src = ptr;
    while (*ptr != '\0' && *ptr != ']') {
        if (*ptr == ';') {
            size_t prop_len = (size_t)(ptr - src);

            if (numofboxreq >= MAX_NUMOFBOX) {
                fprintf(stderr,
                        "JPIP query rejected: metareq field contains more "
                        "than %d box requests\n", MAX_NUMOFBOX);
                return OPJ_FALSE;
            }
            if (prop_len >= sizeof(req_box_prop)) {
                fprintf(stderr,
                        "JPIP query rejected: metareq box property exceeds "
                        "maximum length (%d)\n", MAX_LENOFREOBOXPROP);
                return OPJ_FALSE;
            }
            memcpy(req_box_prop, src, prop_len);
            req_box_prop[prop_len] = '\0';
            parse_req_box_prop(req_box_prop, numofboxreq++, query_param);
            ptr++;
            src = ptr;
            memset(req_box_prop, 0, sizeof(req_box_prop));
        }
        ptr++;
    }

    if (*ptr != ']') {
        fprintf(stderr,
                "Malformed JPIP query: metareq field missing ']' marker\n");
        return OPJ_FALSE;
    }
    {
        size_t prop_len = (size_t)(ptr - src);

        if (numofboxreq >= MAX_NUMOFBOX) {
            fprintf(stderr,
                    "JPIP query rejected: metareq field contains more than "
                    "%d box requests\n", MAX_NUMOFBOX);
            return OPJ_FALSE;
        }
        if (prop_len >= sizeof(req_box_prop)) {
            fprintf(stderr,
                    "JPIP query rejected: metareq box property exceeds "
                    "maximum length (%d)\n", MAX_LENOFREOBOXPROP);
            return OPJ_FALSE;
        }
        memcpy(req_box_prop, src, prop_len);
        req_box_prop[prop_len] = '\0';
    }

    parse_req_box_prop(req_box_prop, numofboxreq++, query_param);

    if ((ptr = strchr(field, 'R'))) {
        sscanf(ptr + 1, "%d", &(query_param->root_bin));
    }

    if ((ptr = strchr(field, 'D'))) {
        sscanf(ptr + 1, "%d", &(query_param->max_depth));
    }

    if ((ptr = strstr(field, "!!"))) {
        query_param->metadata_only = OPJ_TRUE;
    }

    return OPJ_TRUE;
}

void parse_req_box_prop(char *req_box_prop, int idx, query_param_t *query_param)
{
    char *ptr;

    if (idx < 0 || idx >= MAX_NUMOFBOX) {
        fprintf(stderr,
                "JPIP query rejected: metareq box index %d out of range\n",
                idx);
        return;
    }

    if (*req_box_prop == '*') {
        query_param->box_type[idx][0] = '*';
        query_param->box_type[idx][1] = '\0';
    } else {
        /* box_type is a fixed 4-byte fourcc field, historically filled with
         * strncpy(..., 4) and not NUL terminated. Accept up to 4 bytes and
         * only reject when the box name runs past the field without a
         * qualifier separator, which is what would have overflowed. */
        size_t type_len = strcspn(req_box_prop, ":/!");
        if (type_len > sizeof(query_param->box_type[idx])) {
            fprintf(stderr,
                    "JPIP query rejected: metareq box type exceeds maximum "
                    "length (%u)\n",
                    (unsigned)(sizeof(query_param->box_type[idx]) - 1));
            return;
        }
        memset(query_param->box_type[idx], 0,
               sizeof(query_param->box_type[idx]));
        memcpy(query_param->box_type[idx], req_box_prop,
               type_len <= sizeof(query_param->box_type[idx]) ?
               type_len : sizeof(query_param->box_type[idx]));
    }

    if ((ptr = strchr(req_box_prop, ':'))) {
        if (*(ptr + 1) == 'r') {
            query_param->limit[idx] = -1;
        } else {
            sscanf(ptr + 1, "%d", &(query_param->limit[idx]));
        }
    }

    if ((ptr = strchr(req_box_prop, '/'))) {
        ptr++;
        while (*ptr == 'w' || *ptr == 's' || *ptr == 'g' || *ptr == 'a') {
            switch (*ptr) {
            case 'w':
                query_param->w[idx] = OPJ_TRUE;
                break;
            case 's':
                query_param->s[idx] = OPJ_TRUE;
                break;
            case 'g':
                query_param->g[idx] = OPJ_TRUE;
                break;
            case 'a':
                query_param->a[idx] = OPJ_TRUE;
                break;
            }
            ptr++;
        }
    } else {
        query_param->g[idx] = OPJ_TRUE;
        query_param->s[idx] = OPJ_TRUE;
        query_param->w[idx] = OPJ_TRUE;
    }

    if ((ptr = strchr(req_box_prop, '!'))) {
        query_param->priority[idx] = OPJ_TRUE;
    }

    idx++;
}

OPJ_BOOL parse_comps(char *field, query_param_t *query_param)
{
    int i, start = 0, stop = 0, aux = -1;
    int matched;
    char *ptr1, *ptr2;

    if (field == NULL || field[0] == '\0') {
        fprintf(stderr, "Malformed JPIP query: empty comps field\n");
        return OPJ_FALSE;
    }

    ptr1 = strchr(field, '-');
    ptr2 = strchr(field, ',');

    if (ptr1 && ptr2)
        if (ptr1 > ptr2) {
            matched = sscanf(field, "%d,%d-%d", &aux, &start, &stop);
            if (matched != 3) {
                fprintf(stderr,
                        "Malformed JPIP query: comps field is not numeric\n");
                return OPJ_FALSE;
            }
        } else {
            matched = sscanf(field, "%d-%d,%d", &start, &stop, &aux);
            if (matched != 3) {
                fprintf(stderr,
                        "Malformed JPIP query: comps field is not numeric\n");
                return OPJ_FALSE;
            }
        } else if (ptr1) {
        if (sscanf(field, "%d-%d", &start, &stop) != 2) {
            fprintf(stderr,
                    "Malformed JPIP query: comps field is not numeric\n");
            return OPJ_FALSE;
        }
    } else if (ptr2) {
        if (sscanf(field, "%d,%d", &start, &stop) != 2) {
            fprintf(stderr,
                    "Malformed JPIP query: comps field is not numeric\n");
            return OPJ_FALSE;
        }
        aux = start;
        start = stop;
    } else {
        if (sscanf(field, "%d", &stop) != 1) {
            fprintf(stderr,
                    "Malformed JPIP query: comps field is not numeric\n");
            return OPJ_FALSE;
        }
        start = stop;
    }

    if (start < 0 || stop < 0 || start > stop || aux < -1) {
        fprintf(stderr,
                "Malformed JPIP query: invalid component range in comps field\n");
        return OPJ_FALSE;
    }

    if (aux != -1 && aux < 0) {
        fprintf(stderr,
                "Malformed JPIP query: invalid component index in comps field\n");
        return OPJ_FALSE;
    }

    query_param->lastcomp = stop > aux ? stop : aux;
    query_param->comps = (OPJ_BOOL *)opj_calloc(1,
                         (OPJ_SIZE_T)(query_param->lastcomp + 1) * sizeof(OPJ_BOOL));
    if (query_param->comps == NULL) {
        fprintf(stderr,
                "Memory allocation failed while parsing comps field\n");
        query_param->lastcomp = -1;
        return OPJ_FALSE;
    }

    for (i = start; i <= stop; i++) {
        query_param->comps[i] = OPJ_TRUE;
    }

    if (aux != -1) {
        query_param->comps[aux] = OPJ_TRUE;
    }

    return OPJ_TRUE;
}

void delete_query(query_param_t **query)
{
    if (query == NULL || *query == NULL) {
        return;
    }

    if ((*query)->target) {
        opj_free((*query)->target);
    }

    if ((*query)->tid) {
        opj_free((*query)->tid);
    }

    if ((*query)->comps) {
        opj_free((*query)->comps);
    }

    if ((*query)->cid) {
        opj_free((*query)->cid);
    }

    if ((*query)->cclose) {
        opj_free((*query)->cclose);
    }

    opj_free(*query);
}
