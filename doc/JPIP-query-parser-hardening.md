# JPIP Query Parser Hardening

## Background

The JPIP query parser in `src/lib/openjpip/query_parser.c` copied field names
and values from incoming request strings into fixed-size stack buffers with
`strncpy()` calls that did not check the source length against the
destination capacity. A request containing an overlong field (for example a
crafted `target`, `cid` or `metareq` value) therefore overwrote the stack.
The socket line reader (`receive_line`) had the same class of defect and even
carried a source comment acknowledging it.

With files now arriving from external organizations and user uploads rather
than only internal systems, these inputs must be treated as adversarial.

## What changed

### Bounded string primitives (`string_compat.[ch]`)

All length-sensitive string operations in `libopenjpip` go through one of:

- `opj_strlcpy()` - bounded copy, rejects overflow, always NUL-terminates
- `opj_strlcat()` - bounded concatenation, rejects overflow
- `opj_snprintf()` - bounded formatting, treats truncation as an error
- `opj_strncpy()` - bounded prefix copy with explicit capacity

Every helper returns `OPJ_FALSE` on overflow and logs the offending field
name together with the required and available byte counts. No helper silently
truncates. There are no remaining `strcpy`, `strcat`, `sprintf` or `vsprintf`
call sites in `libopenjpip`; the remaining `strncpy` calls copy fixed
four-byte JP2 box types into four-byte fields and cannot overflow.

### Fail-fast parser (`query_parser.[ch]`)

- `parse_query_ex()` is the new entry point and returns a `query_error_t`
  describing the first violation. `parse_query()` is kept as a thin wrapper so
  the existing public API and ABI are unchanged.
- Field names are limited to `MAX_LENOFFIELDNAME` (11) and values to
  `MAX_LENOFFIELDVAL` (129) bytes including the NUL. Boundary-sized values
  still parse exactly as before.
- `metareq` req-box-prop entries are bounded to `MAX_LENOFREQBOXPROP` (20),
  the number of requested boxes is capped at `MAX_NUMOFBOX` (10), and a
  missing closing bracket is rejected instead of scanning off the buffer.
- `comps` rejects negative, out-of-range or malformed component numbers that
  previously caused out-of-bounds writes or unbounded allocations.
- Valid inputs produce byte-identical parse results to the previous code
  (verified by diffing `print_queryparam()` output against the unmodified
  baseline).

### Error taxonomy

`query_error_type_t` identifies each failure category; stable names are
exposed through `query_error_type_name`:

| Type | Meaning |
| --- | --- |
| `malformed` | field is not in `name=value` form |
| `fieldname_too_long` | field name exceeds its buffer |
| `fieldval_too_long` | field value exceeds its buffer |
| `metareq_too_long` | a req-box-prop exceeds its buffer |
| `too_many_boxes` | more than `MAX_NUMOFBOX` boxes requested |
| `metareq_malformed` | metareq has no closing bracket |
| `comps_invalid` | negative/out-of-range component specification |

### Bounded socket input (`sock_manager.[ch]`)

`receive_line_safe()` takes the destination capacity, never writes past it,
and drains the remainder of an overlong line so subsequent reads stay
synchronized. `receive_line()` and `receive_string()` are now safe wrappers;
callers that previously assumed `receive_string()` never returned NULL handle
the failure explicitly.

### Error statistics (`parse_errorlog.[ch]`)

Counters are grouped by `query_error_type_t`, can be written to and merged
from `key=value` files, and rendered as a human-readable report.

## Full archive re-scan

The `opj_jpip_scan` tool re-parses archived query strings (one per line) and
is designed to stream millions of records without aborting on a single bad
one:

```sh
# scan a manifest of archived query strings
opj_jpip_scan -f archive_queries.txt \
              -s parse_stats.txt \
              -o suspect_records.txt

# scan every regular file in a directory tree level
opj_jpip_scan -d /var/archive/jpip_chunks -s parse_stats.txt

# stream from another tool
cat archive_queries.txt | opj_jpip_scan --stdin
```

`suspect_records.txt` lists one line per record that failed to parse, with
the error type, source file, offending field name and the required versus
available lengths. These are the candidates to re-ingest or repair, since
they are the only records that the historical unbounded parser could have
corrupted. The exit status is non-zero when any error is recorded, so the
tool can be wired into cron or CI.

The stats file is mergeable: split the archive across shards, scan each
shard, and concatenate/load the per-shard stats with
`parse_error_stats_load()` to see which error class dominates.

## Verification

- `testjpip_query_parser` - overlong/malformed fields assert a typed error
  without crashing, plus field-by-field checks of a valid query
- `testjpip_sockline` - drives `receive_line_safe()` over a socketpair with
  overlong lines and verifies resynchronization
- Both tests run clean under AddressSanitizer plus UndefinedBehaviorSanitizer,
  and a 200,000-record mixed workload completes with no sanitizer findings
- The full CTest suite shows the same pass/fail set as the unmodified
  baseline (remaining failures are unrelated and due to the optional
  `OPJ_DATA_ROOT` data package not being installed)
