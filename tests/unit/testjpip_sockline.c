/*
 * Security regression tests for the bounded socket line reader.
 *
 * receive_line() used to write attacker-controlled bytes into a fixed
 * BUF_LEN stack buffer with no limit. These tests drive the real reader over
 * a loopback socketpair and assert that an overlong line is rejected without
 * memory corruption while a normal-length line is still returned intact.
 */

#define OPJ_SKIP_POISON

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sock_manager.h"

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", (msg), __LINE__); \
            failures++; \
        } \
    } while (0)

static void send_all(int fd, const char *data, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, data + off, len - off, 0);
        if (n <= 0) {
            return;
        }
        off += (size_t)n;
    }
}

int main(void)
{
    int sv[2];
    char buf[BUF_LEN];

    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair");
        return 1;
    }

    /* 1. a normal line well under the limit is read back unchanged */
    send_all(sv[0], "JPIP-stream\n", 12U);
    CHECK(receive_line_safe(sv[1], buf, sizeof(buf)) == 11U,
          "normal line length");
    CHECK(strcmp(buf, "JPIP-stream") == 0, "normal line content");

    /* 2. an overlong line is rejected and the trailing newline is drained so
     *    the next read stays synchronized */
    {
        char *oversized = (char *)malloc(BUF_LEN + 100U);
        memset(oversized, 'x', BUF_LEN + 99U);
        oversized[BUF_LEN + 99U] = '\n';
        send_all(sv[0], oversized, (size_t)BUF_LEN + 100U);
        free(oversized);

        CHECK(receive_line_safe(sv[1], buf, sizeof(buf)) ==
              (OPJ_SIZE_T)-1, "overlong line rejected");
        CHECK(strlen(buf) == BUF_LEN - 1U,
              "buffer is NUL-terminated at its capacity");
    }

    /* 3. the line following an overlong one is still read correctly */
    send_all(sv[0], "PNM request\n", 12U);
    CHECK(receive_line_safe(sv[1], buf, sizeof(buf)) == 11U,
          "reader resynchronizes after an overlong line");
    CHECK(strcmp(buf, "PNM request") == 0, "resynchronized line content");

    /* 4. receive_string() returns NULL instead of overflowing */
    {
        char *oversized = (char *)malloc(BUF_LEN + 50U);
        char *got;
        memset(oversized, 'y', BUF_LEN + 49U);
        oversized[BUF_LEN + 49U] = '\n';
        send_all(sv[0], oversized, (size_t)BUF_LEN + 50U);
        free(oversized);

        got = receive_string(sv[1]);
        CHECK(got == NULL, "receive_string rejects an overlong line");
        free(got);
    }

    close(sv[0]);
    close(sv[1]);

    if (failures == 0) {
        printf("testjpip_sockline: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "testjpip_sockline: %d check(s) failed\n", failures);
    return 1;
}
