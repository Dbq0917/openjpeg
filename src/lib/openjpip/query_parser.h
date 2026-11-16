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

#ifndef     QUERY_PARSER_H_
# define    QUERY_PARSER_H_

#include "opj_includes.h"

/** maximum number of meta request box */
#define MAX_NUMOFBOX 10

/** maximum length (including the terminating NUL) of a query field name */
#define MAX_LENOFFIELDNAME 11

/** maximum length (including the terminating NUL) of a query field value */
#define MAX_LENOFFIELDVAL 129

/** maximum length (including the terminating NUL) of a single req-box-prop */
#define MAX_LENOFREQBOXPROP 20

/**
 * Categories of parse errors.
 * New values must only be appended so that persisted statistics keep their
 * meaning across releases.
 */
typedef enum query_error_type {
    QERR_NONE = 0,              /**< no error */
    QERR_MALFORMED = 1,         /**< field is not of the name=value form */
    QERR_FIELDNAME_TOO_LONG = 2,/**< field name exceeds MAX_LENOFFIELDNAME */
    QERR_FIELDVAL_TOO_LONG = 3, /**< field value exceeds MAX_LENOFFIELDVAL */
    QERR_METAREQ_TOO_LONG = 4,  /**< a req-box-prop exceeds its buffer */
    QERR_TOO_MANY_BOXES = 5,    /**< more than MAX_NUMOFBOX boxes requested */
    QERR_METAREQ_MALFORMED = 6, /**< metareq field has no closing bracket */
    QERR_COMPS_INVALID = 7      /**< comps field has negative or out of range values */
} query_error_type_t;

/** Number of entries in query_error_type_t (QERR_NONE included) */
#define QUERY_ERROR_TYPE_COUNT 8

/** Short, stable names used when persisting error statistics */
extern const char *const query_error_type_name[QUERY_ERROR_TYPE_COUNT];

/**
 * Parse error record. A parse run owns at most one active error: the first
 * length violation or malformed field aborts parsing.
 */
typedef struct query_error {
    query_error_type_t type;    /**< error category, QERR_NONE when clean */
    char field[MAX_LENOFFIELDNAME]; /**< offending field name, empty when the
                                         malformed input has no name */
    size_t required;            /**< bytes required by the offending field */
    size_t capacity;            /**< bytes available in the target buffer */
} query_error_t;

/** cnew aux transport name */
typedef enum cnew_transport { non, http, tcp, udp} cnew_transport_t;

/** image return type */
typedef enum image_return { JPPstream, JPTstream, UNKNOWN = -1} image_return_t;

/** Query parameters */
typedef struct query_param {
    char *target;                               /**< target name */
    char *tid;                                  /**< target identifier */
    int fx, fy;                                 /**< frame size (fx,fy) */
    int rx, ry, rw, rh;                         /**< roi region */
    int layers;                                 /**< quality layers */
    int lastcomp;                               /**< last component number */
    OPJ_BOOL *comps;                                /**< components (dynamic array) for jpp-stream, null means all components */
    char *cid;                                  /**< channel identifier */
    cnew_transport_t
    cnew;                      /**< transport name if there is new channel request, else non */
    char *cclose;                               /**< list of closing channel identifiers, separated by '\\0' */
    int numOfcclose;                            /**< number of closing channels */
    char box_type[MAX_NUMOFBOX][4];             /**< interested box-types */
    int limit[MAX_NUMOFBOX];                    /**< limit value, -1: skeleton request "r", 0: entire contents */
    OPJ_BOOL w[MAX_NUMOFBOX];                       /**< Metadata request qualifier flags */
    OPJ_BOOL s[MAX_NUMOFBOX];
    OPJ_BOOL g[MAX_NUMOFBOX];
    OPJ_BOOL a[MAX_NUMOFBOX];
    OPJ_BOOL priority[MAX_NUMOFBOX];                /**< priority flag */
    int root_bin;                               /**< root-bin */
    int max_depth;                              /**< max-depth */
    OPJ_BOOL metadata_only;                         /**< metadata-only request */
    image_return_t return_type;                 /**< image return type */
    int len;                                    /**< maximum response length */
} query_param_t;


/**
 * parse query
 *
 * @param[in]  query_string request query string
 * @return     pointer to query parameters
 *
 * Kept for backward compatibility. Prefer parse_query_ex() in new code so
 * that overlong or malformed fields are reported rather than only logged.
 */
query_param_t * parse_query(const char *query_string);

/**
 * parse query with explicit error reporting
 *
 * Every field copied into a fixed-size buffer is length-checked first. On the
 * first violation parsing stops immediately, the error (including the
 * offending field name and the required versus available lengths) is stored in
 * @p error and the partially populated query parameters are returned for
 * cleanup. Valid inputs produce exactly the same result as parse_query().
 *
 * @param[in]  query_string request query string
 * @param[out] error        parse error record, reset to QERR_NONE on entry
 * @return     pointer to query parameters (never NULL; caller must still run
 *             delete_query() even when an error is reported)
 */
query_param_t * parse_query_ex(const char *query_string, query_error_t *error);

/**
 * print query parameters
 *
 * @param[in] query_param  query parameters
 */
void print_queryparam(query_param_t query_param);


/**
 * delete query
 *
 * @param[in] query address of the deleting query pointer
 */
void delete_query(query_param_t **query);

#endif      /* !QUERY_PARSER_H_ */
