/*
 * Host-side tests for bridge_protocol.h (no STM32 required).
 *
 * Build with any C11 compiler, e.g.:
 *   zig cc -std=c11 -Wall -Wextra -Werror test_protocol.c -o test_protocol
 *   ./test_protocol
 *
 * Prints:
 *   FRAME <hex>   a real frame for payload {"id":"c1","op":"status"}
 *   CRC <hex>     CRC-16/CCITT-FALSE of "123456789"
 * so the Python implementation can be checked against the C one.
 */

#include "bridge_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if(!(cond)) {                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while(0)

static void print_hex(const uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) printf("%02x", data[i]);
}

int main(void) {
    /* CRC known vector */
    const uint8_t vector[] = "123456789";
    uint16_t crc = bridge_crc16(vector, 9);
    CHECK(crc == 0x29B1);
    printf("CRC %04x\n", crc);

    /* COBS round-trips */
    uint8_t data[1200];
    uint8_t enc[1400];
    uint8_t dec[1200];
    size_t cases[] = {0, 1, 2, 256, 1000, 255, 254};
    for(size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        size_t n = cases[c];
        for(size_t i = 0; i < n; i++) {
            data[i] = (uint8_t)((i * 37u + c * 11u) & 0xFF);
        }
        if(c == 1) data[0] = 0x00;
        if(c == 2) {
            data[0] = 0x00;
            data[1] = 0x00;
        }
        if(c == 5) {
            for(size_t i = 0; i < n; i++) data[i] = 0x11;
        }
        if(c == 6) {
            for(size_t i = 0; i < n; i++) data[i] = (i == 0) ? 0x00 : 0x22;
        }

        size_t elen = bridge_cobs_encode(data, n, enc, sizeof(enc));
        CHECK(elen > 0);
        for(size_t i = 0; i < elen; i++) CHECK(enc[i] != 0x00);

        size_t dlen = bridge_cobs_decode(enc, elen, dec, sizeof(dec));
        CHECK(dlen == n);
        CHECK(n == 0 || memcmp(data, dec, n) == 0);
    }

    /* Frame round-trip with embedded zeros */
    const char* json = "{\"id\":\"c1\",\"op\":\"status\"}";
    uint8_t frame[1400];
    size_t flen = bridge_frame_build(
        BridgeTypeCmd, 42, BRIDGE_FLAG_ACK_REQ, (const uint8_t*)json, (uint16_t)strlen(json),
        frame, sizeof(frame));
    CHECK(flen > 0);
    CHECK(frame[flen - 1] == 0x00);

    BridgeFrame parsed;
    CHECK(bridge_frame_parse(frame, flen - 1, dec, sizeof(dec), &parsed));
    CHECK(parsed.type == BridgeTypeCmd);
    CHECK(parsed.seq == 42);
    CHECK(parsed.flags == BRIDGE_FLAG_ACK_REQ);
    CHECK(parsed.payload_len == strlen(json));
    CHECK(memcmp(parsed.payload, json, strlen(json)) == 0);

    printf("FRAME ");
    print_hex(frame, flen);
    printf("\n");

    /* Corrupt one byte: parse must fail (CRC covers the header too) */
    frame[flen / 2] ^= 0x5A;
    CHECK(!bridge_frame_parse(frame, flen - 1, dec, sizeof(dec), &parsed));

    /* Maximum payload */
    static uint8_t big[BRIDGE_MAX_PAYLOAD];
    memset(big, 'x', sizeof(big));
    flen = bridge_frame_build(BridgeTypeAiResp, 1, 0, big, BRIDGE_MAX_PAYLOAD, frame, sizeof(frame));
    CHECK(flen > 0);
    CHECK(bridge_frame_parse(frame, flen - 1, dec, sizeof(dec), &parsed));
    CHECK(parsed.payload_len == BRIDGE_MAX_PAYLOAD);

    if(failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("C protocol tests passed\n");
    return 0;
}
