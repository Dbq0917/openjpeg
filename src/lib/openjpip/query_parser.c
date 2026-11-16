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
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "query_parser.h"
#include "string_compat.h"

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

/*
 * get a pair of field name and value from the string starting fieldname=fieldval&... format
 *
 * @param[in] stringptr pointer to the beginning of the parsing string
 * @param[out] fieldname buffer of MAX_LENOFFIELDNAME bytes
 * @param[out] fieldval  buffer of MAX_LENOFFIELDVAL bytes
 * @param[out] error     parse error record, set on overflow or malformed input
 * @return pointer to the next field string, NULL when there is none or an
 *         error occurred (check error->type to distinguish the two cases)
 */
char * get_fieldparam(const char *stringptr, char *fieldname, char *fieldval,
                      query_error_t *error);

void parse_cclose(char *src, query_param_t *query_param);
OPJ_BOOL parse_metareq(char *field, query_param_t *query_param,
                       query_error_t *error);

/* parse the requested components (parses forms like:a; a,b; a-b; a-b,c;  a,b-c)*/
OPJ_BOOL parse_comps(char *field, query_param_t *query_param);

const char *const query_error_type_name[QUERY_ERROR_TYPE_COUNT] = {
    "none",
    "malformed",
    "fieldname_too_long",
    "fieldval_too_long",
    "metareq_too_long",
    "too_many_boxes",
    "metareq_malformed",
    "comps_invalid"
};

static void set_query_error(query_error_t *error, query_error_type_t type,
                            const char *field, size_t required,
                            size_t capacity)
{
    if (error == NULL || error->type != QERR_NONE) {
        return;
    }
    error->type = type;
    error->required = required;
    error->capacity = capacity;
    error->field[0] = '\0';
    if (field != NULL && field[0] != '\0' &&
            strlen(field) < sizeof(error->field)) {
        /* only record the field name when it fits; an overlong name is itself
         * the violation and is reported through the error type/lengths */
        (void)opj_strlcpy(error->field, sizeof(error->field), field,
                          "error.field");
    }
}

query_param_t * parse_query_ex(const char *query_string, query_error_t *error)
{
    query_param_t *query_param;
    const char *pquery;
    char fieldname[MAX_LENOFFIELDNAME], fieldval[MAX_LENOFFIELDVAL];

    if (error != NULL) {
        error->type = QERR_NONE;
        error->field[0] = '\0';
        error->required = 0;
        error->capacity = 0;
    }

    query_param = get_initquery();

    if (query_string == NULL) {
        set_query_error(error, QERR_MALFORMED, "", 0U, 0U);
        return query_param;
    }

    pquery = query_string;

    while (pquery != NULL && (error == NULL || error->type == QERR_NONE)) {

        pquery = get_fieldparam(pquery, fieldname, fieldval, error);

        if (error != NULL && error->type != QERR_NONE) {
            break;
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
                parse_cclose(fieldval, query_param);
            }

            else if (strcasecmp(fieldname, "metareq") == 0) {
                if (!parse_metareq(fieldval, query_param, error)) {
                    if (error != NULL && error->field[0] == '\0') {
                        set_query_error(error, error->type, "metareq",
                                        error->required, error->capacity);
                    }
                }
            }

            else if (strcasecmp(fieldname, "comps") == 0) {
                if (!parse_comps(fieldval, query_param)) {
                    set_query_error(error, QERR_COMPS_INVALID, "comps",
                                    (size_t)strlen(fieldval) + 1U,
                                    (size_t)MAX_LENOFFIELDVAL);
                    break;
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
                    strncpy(query_param->box_type[0], "ftyp", 4);
                }
            }
        }
    }
    return query_param;
}

query_param_t * parse_query(const char *query_string)
{
    query_error_t error;
    query_param_t *query_param = parse_query_ex(query_string, &error);

    if (error.type != QERR_NONE) {
        fprintf(stderr,
                "Error: query parsing aborted (%s): field '%s' requires %u "
                "byte(s), capacity is %u\n",
                query_error_type_name[error.type],
                error.field[0] ? error.field : "(unknown)",
                (unsigned)error.required, (unsigned)error.capacity);
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


char * get_fieldparam(const char *stringptr, char *fieldname, char *fieldval,
                      query_error_t *error)
{
    const char *eqp, *andp, *nexfieldptr;
    size_t namelen, vallen;

    if ((eqp = strchr(stringptr, '=')) == NULL) {
        set_query_error(error, QERR_MALFORMED, "", 1U, 0U);
        fieldname[0] = '\0';
        fieldval[0] = '\0';
        return NULL;
    }
    if ((andp = strchr(stringptr, '&')) == NULL) {
        andp = strchr(stringptr, '\0');
        nexfieldptr = NULL;
    } else {
        nexfieldptr = andp + 1;
    }

    /* a field name must be non-empty */
    if (eqp == stringptr) {
        set_query_error(error, QERR_MALFORMED, "", 1U, 0U);
        return NULL;
    }
    if (andp < eqp) { /* a '&' before '=' belongs to the next field pair */
        set_query_error(error, QERR_MALFORMED, "", 1U, 0U);
        return NULL;
    }

    namelen = (size_t)(eqp - stringptr);
    vallen = (size_t)(andp - eqp - 1);

    if (namelen + 1U > MAX_LENOFFIELDNAME) {
        set_query_error(error, QERR_FIELDNAME_TOO_LONG, stringptr,
                        namelen + 1U, (size_t)MAX_LENOFFIELDNAME);
        return NULL;
    }

    if (!opj_strncpy(fieldname, (size_t)MAX_LENOFFIELDNAME, stringptr,
                     namelen, "fieldname")) {
        set_query_error(error, QERR_FIELDNAME_TOO_LONG, stringptr,
                        namelen + 1U, (size_t)MAX_LENOFFIELDNAME);
        return NULL;
    }

    if (vallen + 1U > MAX_LENOFFIELDVAL) {
        /* report the value overflow against the now-known field name */
        set_query_error(error, QERR_FIELDVAL_TOO_LONG, fieldname,
                        vallen + 1U, (size_t)MAX_LENOFFIELDVAL);
        return NULL;
    }

    if (!opj_strncpy(fieldval, (size_t)MAX_LENOFFIELDVAL, eqp + 1, vallen,
                     fieldname)) {
        set_query_error(error, QERR_FIELDVAL_TOO_LONG, fieldname,
                        vallen + 1U, (size_t)MAX_LENOFFIELDVAL);
        return NULL;
    }

    return (char *)nexfieldptr;
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

void parse_cclose(char *src, query_param_t *query_param)
{
    size_t i;
    size_t len;

    len = strlen(src);
    query_param->cclose = strdup(src);

    for (i = 0; i < len; i++)
        if (query_param->cclose[i] == ',') {
            query_param->cclose[i] = '\0';
            query_param->numOfcclose ++;
        }

    query_param->numOfcclose ++;
}

void parse_req_box_prop(char *req_box_prop, int idx,
                        query_param_t *query_param);

OPJ_BOOL parse_metareq(char *field, query_param_t *query_param,
                       query_error_t *error)
{
    char req_box_prop[MAX_LENOFREQBOXPROP];
    char *ptr, *src;
    int numofboxreq = 0;
    size_t proplen;

    memset(req_box_prop, 0, sizeof(req_box_prop));

    /* req-box-prop*/
    ptr = strchr(field, '[');
    if (ptr == NULL) {
        set_query_error(error, QERR_METAREQ_MALFORMED, "metareq",
                        strlen(field) + 1U, sizeof(req_box_prop));
        return OPJ_FALSE;
    }
    ptr++;
    src = ptr;
    while (*ptr != '\0' && *ptr != ']') {
        if (*ptr == ';') {
            proplen = (size_t)(ptr - src);
            if (proplen + 1U > sizeof(req_box_prop)) {
                set_query_error(error, QERR_METAREQ_TOO_LONG, "metareq",
                                proplen + 1U, sizeof(req_box_prop));
                return OPJ_FALSE;
            }
            if (numofboxreq >= MAX_NUMOFBOX) {
                set_query_error(error, QERR_TOO_MANY_BOXES, "metareq",
                                (size_t)(numofboxreq + 1),
                                (size_t)MAX_NUMOFBOX);
                return OPJ_FALSE;
            }
            if (!opj_strncpy(req_box_prop, sizeof(req_box_prop), src, proplen,
                             "metareq")) {
                set_query_error(error, QERR_METAREQ_TOO_LONG, "metareq",
                                proplen + 1U, sizeof(req_box_prop));
                return OPJ_FALSE;
            }
            parse_req_box_prop(req_box_prop, numofboxreq++, query_param);
            ptr++;
            src = ptr;
            memset(req_box_prop, 0, sizeof(req_box_prop));
        }
        ptr++;
    }

    if (*ptr != ']') {
        set_query_error(error, QERR_METAREQ_MALFORMED, "metareq",
                        strlen(field) + 1U, sizeof(req_box_prop));
        return OPJ_FALSE;
    }

    proplen = (size_t)(ptr - src);
    if (proplen + 1U > sizeof(req_box_prop)) {
        set_query_error(error, QERR_METAREQ_TOO_LONG, "metareq",
                        proplen + 1U, sizeof(req_box_prop));
        return OPJ_FALSE;
    }
    if (numofboxreq >= MAX_NUMOFBOX) {
        set_query_error(error, QERR_TOO_MANY_BOXES, "metareq",
                        (size_t)(numofboxreq + 1), (size_t)MAX_NUMOFBOX);
        return OPJ_FALSE;
    }
    if (!opj_strncpy(req_box_prop, sizeof(req_box_prop), src, proplen,
                     "metareq")) {
        set_query_error(error, QERR_METAREQ_TOO_LONG, "metareq",
                        proplen + 1U, sizeof(req_box_prop));
        return OPJ_FALSE;
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
        return;
    }

    if (*req_box_prop == '*') {
        query_param->box_type[idx][0] = '*';
    } else {
        size_t typelen = strcspn(req_box_prop, ":/!");
        if (typelen > 4U) {
            typelen = 4U;
        }
        /* box_type is a fixed 4-byte, non-NUL-terminated JP2 box type field */
        memcpy(query_param->box_type[idx], req_box_prop, typelen);
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
    int i, start, stop, aux = -1;
    char *ptr1, *ptr2;
    int matched;

#define MAX_COMP_NUMBER 65535

    ptr1 = strchr(field, '-');
    ptr2 = strchr(field, ',');

    start = stop = -1;

    if (ptr1 && ptr2) {
        if (ptr1 > ptr2) {
            matched = sscanf(field, "%d,%d-%d", &aux, &start, &stop);
        } else {
            matched = sscanf(field, "%d-%d,%d", &start, &stop, &aux);
        }
    } else if (ptr1) {
        matched = sscanf(field, "%d-%d", &start, &stop);
    } else if (ptr2) {
        matched = sscanf(field, "%d,%d", &start, &stop);
        aux = start;
        start = stop;
    } else {
        matched = sscanf(field, "%d", &stop);
        start = stop;
    }

    if (matched < 1 || start < 0 || stop < 0 || aux < -1 ||
            start > MAX_COMP_NUMBER || stop > MAX_COMP_NUMBER ||
            aux > MAX_COMP_NUMBER || start > stop) {
        fprintf(stderr, "Error: invalid comps field '%s'\n", field);
        return OPJ_FALSE;
    }

    query_param->lastcomp = stop > aux ? stop : aux;
    query_param->comps = (OPJ_BOOL *)opj_calloc(1,
                         (OPJ_SIZE_T)(query_param->lastcomp + 1) * sizeof(OPJ_BOOL));

    if (query_param->comps == NULL) {
        fprintf(stderr, "Error: failed to allocate comps array\n");
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

#undef MAX_COMP_NUMBER
}

void delete_query(query_param_t **query)
{
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
