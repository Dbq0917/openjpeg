#!/usr/bin/env python3
# Acceptance test for the opj_dec_server request-line length checks.
#
# Usage: jpip_dec_server_test.py <path-to-opj_dec_server> <path-to-opj_compress>
#
# Scenario:
#   1. send an overlong first request line: the server must reject it with
#      the explicit error code JPIP_RESPONSE_LINETOOLONG (2) and stay alive;
#   2. send overlong lines on the other protocol branches (JPIP-stream
#      handshake header, PNM request): same explicit rejection;
#   3. run a normal handshake (JPIP-stream message) and an image fetch
#      (PNM request): both must succeed, proving that normal connections
#      are not affected and that a rejected request does not pollute the
#      next ones.

import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

JPIP_RESPONSE_OK = 1
JPIP_RESPONSE_LINETOOLONG = 2

IMG_W = 16
IMG_H = 16
TARGET = "test.jp2"
TID = "tid-1"
CID = "cid-1"


def vbas_encode(value):
    out = bytearray()
    while True:
        out.insert(0, value & 0x7F)
        value >>= 7
        if not value:
            break
    for i in range(len(out) - 1):
        out[i] |= 0x80
    return bytes(out)


def jpt_message(in_class_id, class_id, csn, bin_offset, body):
    bin_id = bytes([(3 << 5) | (1 << 4) | in_class_id])
    return (bin_id + vbas_encode(class_id) + vbas_encode(csn)
            + vbas_encode(bin_offset) + vbas_encode(len(body)) + body)


def build_jpt_stream(j2k):
    sot = j2k.find(b"\xff\x90")
    assert sot > 0, "no SOT marker found in j2k codestream"
    assert j2k.endswith(b"\xff\xd9"), "no EOC marker in j2k codestream"
    mainhead = j2k[:sot]
    tiledata = j2k[sot:-2]
    stream = (jpt_message(0, 6, 0, 0, mainhead)   # MAINHEADER_MSG
              + jpt_message(0, 4, 0, 0, tiledata)  # TILE_MSG
              + b"\x00\x01\x00")                   # EOR
    return stream


def recv_exactly(conn, size):
    data = b""
    while len(data) < size:
        chunk = conn.recv(size - len(data))
        if not chunk:
            raise AssertionError("connection closed while expecting %d bytes"
                                 % size)
        data += chunk
    return data


def response_code(port, payload):
    with socket.create_connection(("127.0.0.1", port), timeout=10) as conn:
        conn.sendall(payload)
        return recv_exactly(conn, 1)[0]


def main():
    dec_server = os.path.abspath(sys.argv[1])
    compress = os.path.abspath(sys.argv[2])
    workdir = tempfile.mkdtemp(prefix="jpip_dec_server_test")

    # generate a tiny valid j2k codestream
    pgm = os.path.join(workdir, "in.pgm")
    j2k = os.path.join(workdir, "out.j2k")
    with open(pgm, "wb") as fp:
        fp.write(b"P5\n%d %d\n255\n" % (IMG_W, IMG_H))
        fp.write(bytes((i * 7) % 256 for i in range(IMG_W * IMG_H)))
    subprocess.check_call([compress, "-i", pgm, "-o", j2k, "-n", "1"],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(j2k, "rb") as fp:
        stream = build_jpt_stream(fp.read())

    # start the server on a free port
    probe = socket.socket()
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
    probe.close()

    log = open(os.path.join(workdir, "server.log"), "wb")
    server = subprocess.Popen([dec_server, str(port)], cwd=workdir,
                              stdout=log, stderr=log)
    try:
        deadline = time.time() + 10
        while True:
            try:
                socket.create_connection(("127.0.0.1", port),
                                         timeout=1).close()
                break
            except OSError:
                if server.poll() is not None:
                    raise AssertionError("opj_dec_server exited at startup")
                if time.time() > deadline:
                    raise AssertionError("opj_dec_server did not start")
                time.sleep(0.1)

        overlong = b"A" * 5000 + b"\n"

        # 1. overlong first request line: rejected with explicit code
        code = response_code(port, overlong)
        assert code == JPIP_RESPONSE_LINETOOLONG, \
            "overlong first line: expected code 2, got %d" % code
        assert server.poll() is None, "server died after overlong first line"

        # 2a. overlong header line inside the JPIP-stream handshake
        code = response_code(port, b"JPIP-stream\n" + overlong)
        assert code == JPIP_RESPONSE_LINETOOLONG, \
            "overlong handshake line: expected code 2, got %d" % code
        assert server.poll() is None, "server died after overlong handshake"

        # 2b. overlong identifier line inside a PNM request
        code = response_code(port, b"PNM request\n" + overlong)
        assert code == JPIP_RESPONSE_LINETOOLONG, \
            "overlong PNM line: expected code 2, got %d" % code
        assert server.poll() is None, "server died after overlong PNM line"

        # 3a. normal handshake: cache a JPT-stream
        handshake = (b"JPIP-stream\nversion 1.2\n" + TARGET.encode() + b"\n"
                     + TID.encode() + b"\n" + CID.encode() + b"\n"
                     + str(len(stream)).encode() + b"\n" + stream)
        code = response_code(port, handshake)
        assert code == JPIP_RESPONSE_OK, \
            "handshake: expected code 1, got %d" % code

        # 3b. normal image fetch: PNM request
        with socket.create_connection(("127.0.0.1", port), timeout=10) as conn:
            conn.sendall(b"PNM request\n" + CID.encode() + b"\n"
                         + b"%d\n%d\n" % (IMG_W, IMG_H))
            header = recv_exactly(conn, 7)
            assert header[0:1] == b"P" and header[1] == 5, \
                "PNM: bad header %r" % header
            width = (header[2] << 8) | header[3]
            height = (header[4] << 8) | header[5]
            assert (width, height) == (IMG_W, IMG_H), \
                "PNM: bad geometry %dx%d" % (width, height)
            pixels = recv_exactly(conn, width * height)
            assert len(pixels) == IMG_W * IMG_H

        # 4. rejections did not pollute the server: reject once more,
        #    then a fresh handshake must still succeed
        code = response_code(port, overlong)
        assert code == JPIP_RESPONSE_LINETOOLONG
        code = response_code(port, handshake)
        assert code == JPIP_RESPONSE_OK, \
            "handshake after rejection: expected code 1, got %d" % code
        assert server.poll() is None, "server died during the test"
    finally:
        server.send_signal(signal.SIGTERM)
        server.wait(timeout=10)
        log.close()
        shutil.rmtree(workdir, ignore_errors=True)

    print("ok: overlong request lines rejected with code 2, "
          "handshake and PNM fetch successful")
    return 0


if __name__ == "__main__":
    sys.exit(main())
