/*
 * Security regression tests for the JPIP query parser.
 *
 * These tests feed overlong and malformed fields that used to overflow the
 * fixed-size fieldname/fieldval/req_box_prop stack buffers, and assert that
 * parsing now reports a typed error instead of corrupting memory. A second
 * group parses well-formed queries and checks every populated field byte by
 * byte against the expected values, guarding the "normal input unchanged"
 * requirement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the library headers poison malloc/free to enforce opj_malloc internally;
 * the test exercises the public API and legitimately uses libc allocation */
#define OPJ_SKIP_POISON

#include "query_parser.h"
#include "parse_errorlog.h"
#include "string_compat.h"

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", (msg), __LINE__); \
            failures++; \
        } \
    } while (0)

static char *repeat_char(char ch, size_t count)
{
    char *buf = (char *)malloc(count + 1U);
    if (buf == NULL) {
        return NULL;
    }
    memset(buf, ch, count);
    buf[count] = '\0';
    return buf;
}

static void expect_error(const char *query, query_error_type_t expected,
                         const char *expected_field)
{
    query_error_t error;
    query_param_t *q = parse_query_ex(query, &error);

    CHECK(q != NULL, "parser must always return a query structure");
    CHECK(error.type == expected, "unexpected error type");
    if (expected_field != NULL && expected_field[0] != '\0' &&
            strlen(expected_field) < sizeof(error.field)) {
        CHECK(strncmp(error.field, expected_field,
                      strlen(expected_field)) == 0,
              "error must name the offending field");
    }
    if (error.type == QERR_FIELDNAME_TOO_LONG ||
            error.type == QERR_FIELDVAL_TOO_LONG ||
            error.type == QERR_METAREQ_TOO_LONG) {
        CHECK(error.required > error.capacity,
              "length error must carry required/capacity lengths");
    }
    delete_query(&q);
}

static void test_overlong_fields(void)
{
    char *long_name = repeat_char('a', MAX_LENOFFIELDNAME + 20);
    char *long_val = repeat_char('b', MAX_LENOFFIELDVAL + 20);
    char *query;

    CHECK(long_name != NULL && long_val != NULL, "allocation");

    /* room for the longest field framing used below ("metareq=[...]") */
    query = (char *)malloc(strlen(long_val) + 32U);
    CHECK(query != NULL, "allocation");

    /* field name longer than its 11-byte buffer */
    sprintf(query, "%s=value", long_name);
    expect_error(query, QERR_FIELDNAME_TOO_LONG, "");

    /* target value longer than the 129-byte field value buffer */
    sprintf(query, "target=%s", long_val);
    expect_error(query, QERR_FIELDVAL_TOO_LONG, "target");

    /* cid overlong */
    sprintf(query, "cid=%s", long_val);
    expect_error(query, QERR_FIELDVAL_TOO_LONG, "cid");

    free(query);
    free(long_name);
    free(long_val);

    /* metareq value fits the field buffer but a single req-box-prop exceeds
     * its 20-byte stack buffer (used to overflow req_box_prop) */
    expect_error("metareq=[abcdefghijklmnopqrst:0]", QERR_METAREQ_TOO_LONG,
                 "metareq");

    /* more than MAX_NUMOFBOX box requests */
    expect_error(
        "metareq=[a:0;b:0;c:0;d:0;e:0;f:0;g:0;h:0;i:0;j:0;k:0]",
        QERR_TOO_MANY_BOXES, "metareq");

    /* malformed: no '=' */
    expect_error("garbage", QERR_MALFORMED, "");

    /* malformed metareq missing the closing bracket */
    expect_error("metareq=[ftyp:0", QERR_METAREQ_MALFORMED, "metareq");

    /* comps with a negative index used to index before the array */
    expect_error("comps=-5", QERR_COMPS_INVALID, "comps");

    /* comps with a huge index used to trigger a gigantic allocation */
    expect_error("comps=0-2147483647", QERR_COMPS_INVALID, "comps");
}

static void test_valid_query_unchanged(void)
{
    const char *query_string =
        "target=img.jp2&fsiz=512,256&roff=10,20&rsiz=100,200&layers=3"
        "&cnew=http-tcp&type=jpp-stream&len=1500&comps=1,3-5"
        "&metareq=[ftyp:r;jp2h/!]R2D4!!&cclose=cidA,cidB";
    query_error_t error;
    query_param_t *q = parse_query_ex(query_string, &error);

    CHECK(error.type == QERR_NONE, "valid query must not report an error");
    CHECK(q->target != NULL && strcmp(q->target, "img.jp2") == 0,
          "target field");
    CHECK(q->fx == 512 && q->fy == 256, "fsiz field");
    CHECK(q->rx == 10 && q->ry == 20, "roff field");
    CHECK(q->rw == 100 && q->rh == 200, "rsiz field");
    CHECK(q->layers == 3, "layers field");
    CHECK(q->cnew == tcp, "cnew http-tcp maps to tcp");
    CHECK(q->return_type == JPPstream, "type field");
    CHECK(q->len == 1500, "len field");
    CHECK(q->lastcomp == 5, "comps last component");
    CHECK(q->comps != NULL && q->comps[1] == OPJ_TRUE &&
          q->comps[2] == OPJ_FALSE && q->comps[3] == OPJ_TRUE &&
          q->comps[4] == OPJ_TRUE && q->comps[5] == OPJ_TRUE,
          "comps bit array");
    CHECK(q->numOfcclose == 2, "cclose count");
    CHECK(q->cclose != NULL && strcmp(q->cclose, "cidA") == 0,
          "first cclose entry");
    CHECK(strcmp(q->cclose + 5, "cidB") == 0, "second cclose entry");

    CHECK(strncmp(q->box_type[0], "ftyp", 4) == 0, "first metareq box type");
    CHECK(q->limit[0] == -1, "ftyp skeleton request limit");
    CHECK(strncmp(q->box_type[1], "jp2h", 4) == 0, "second metareq box type");
    CHECK(q->priority[1] == OPJ_TRUE, "jp2h priority flag");
    CHECK(q->root_bin == 2, "root-bin");
    CHECK(q->max_depth == 4, "max-depth");
    CHECK(q->metadata_only == OPJ_TRUE, "metadata-only marker");

    delete_query(&q);
}

static void test_boundary_lengths_still_valid(void)
{
    /* values exactly at the buffer limit (including the NUL) must succeed */
    char *max_name = repeat_char('t', MAX_LENOFFIELDNAME - 1U);
    char *max_val = repeat_char('v', MAX_LENOFFIELDVAL - 1U);
    char *query;
    query_error_t error;
    query_param_t *q;

    query = (char *)malloc(strlen(max_name) + strlen(max_val) + 2U);
    sprintf(query, "%s=%s", max_name, max_val);
    q = parse_query_ex(query, &error);
    CHECK(error.type == QERR_NONE, "exact-limit values must be accepted");
    delete_query(&q);

    free(query);
    free(max_name);
    free(max_val);
}

static void test_error_statistics(void)
{
    parse_error_stats_t stats;
    char *long_val = repeat_char('z', MAX_LENOFFIELDVAL + 5U);
    char *query = (char *)malloc(strlen(long_val) + 16U);
    query_error_t error;
    query_param_t *q;

    parse_error_stats_init(&stats);

    q = parse_query_ex("target=ok.jp2", &error);
    parse_error_stats_record(&stats, &error);
    delete_query(&q);

    sprintf(query, "target=%s", long_val);
    q = parse_query_ex(query, &error);
    parse_error_stats_record(&stats, &error);
    delete_query(&q);

    q = parse_query_ex(query, &error);
    parse_error_stats_record(&stats, &error);
    delete_query(&q);

    CHECK(stats.count[QERR_NONE] == 1, "success counter");
    CHECK(stats.count[QERR_FIELDVAL_TOO_LONG] == 2,
          "overlong value counter grouped by type");

    CHECK(parse_error_stats_save(&stats, "jpip_parse_error_stats.txt"),
          "stats file save");
    {
        parse_error_stats_t loaded;
        parse_error_stats_init(&loaded);
        CHECK(parse_error_stats_load(&loaded,
                                     "jpip_parse_error_stats.txt"),
              "stats file load");
        CHECK(loaded.count[QERR_FIELDVAL_TOO_LONG] == 2,
              "loaded counters match");
    }

    free(query);
    free(long_val);
}

static void test_safe_string_helpers(void)
{
    char small[8];
    char box[6];

    CHECK(opj_strlcpy(small, sizeof(small), "1234567", "x") == OPJ_TRUE,
          "strlcpy exact fit");
    CHECK(opj_strlcpy(small, sizeof(small), "12345678", "x") == OPJ_FALSE,
          "strlcpy overflow reported");
    CHECK(small[7] == '\0', "strlcpy keeps NUL termination on overflow");

    CHECK(opj_strlcat(small, sizeof(small), "!", "x") == OPJ_FALSE,
          "strlcat overflow reported");
    CHECK(opj_strlcat(small, sizeof(small), "", "x") == OPJ_TRUE,
          "strlcat empty append");

    CHECK(opj_snprintf(small, sizeof(small), "x", "%d", 42) == OPJ_TRUE,
          "snprintf fit");
    CHECK(opj_snprintf(small, sizeof(small), "x", "%d", 12345678) == OPJ_FALSE,
          "snprintf truncation reported");

    CHECK(opj_strncpy(box, sizeof(box), "abcd", 4, "x") == OPJ_TRUE,
          "strncpy bounded fit");
    CHECK(opj_strncpy(box, sizeof(box), "abcdef", 6, "x") == OPJ_FALSE,
          "strncpy bounded overflow reported");
}

int main(void)
{
    test_safe_string_helpers();
    test_overlong_fields();
    test_valid_query_unchanged();
    test_boundary_lengths_still_valid();
    test_error_statistics();

    if (failures == 0) {
        printf("testjpip_query_parser: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "testjpip_query_parser: %d check(s) failed\n", failures);
    return 1;
}
