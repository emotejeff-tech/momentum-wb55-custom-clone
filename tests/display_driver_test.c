/*
 * Behavioural host test for st7567_i2c.c.
 *
 * Replaces the Furi HAL I2C calls with a recorder and checks the exact byte
 * stream the GM12864-59N would see: address 0x3F, control byte 0x00/0x40,
 * the documented init sequence, page/column addressing, clipping and fill.
 *
 * Build (see tests/run_all.py):
 *   zig cc -std=c11 -Wall -Wextra -Werror -I tests/host_stubs \
 *          -I firmware/display_driver_patch \
 *          tests/display_driver_test.c firmware/display_driver_patch/st7567_i2c.c
 */

#include <furi.h>
#include <furi_hal_i2c.h>

#include "st7567_i2c.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Furi HAL stubs                                                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint8_t address;
    uint8_t data[300];
    size_t size;
} RecordedTx;

static RecordedTx g_tx[512];
static int g_tx_count = 0;
static int g_delay_ms = 0;
static int g_delay_us = 0;

const FuriHalI2cBusHandle furi_hal_i2c_handle_power = {0};

void furi_hal_i2c_acquire(const FuriHalI2cBusHandle* handle) {
    (void)handle;
}
void furi_hal_i2c_release(const FuriHalI2cBusHandle* handle) {
    (void)handle;
}

bool furi_hal_i2c_tx(
    const FuriHalI2cBusHandle* handle,
    uint8_t address,
    const uint8_t* data,
    size_t size,
    uint32_t timeout) {
    (void)handle;
    (void)timeout;
    if(g_tx_count >= (int)(sizeof(g_tx) / sizeof(g_tx[0]))) return false;
    if(size > sizeof(g_tx[0].data)) return false;
    g_tx[g_tx_count].address = address;
    g_tx[g_tx_count].size = size;
    memcpy(g_tx[g_tx_count].data, data, size);
    g_tx_count++;
    return true;
}

void furi_delay_ms(uint32_t ms) {
    (void)ms;
    g_delay_ms++;
}
void furi_delay_us(uint32_t us) {
    (void)us;
    g_delay_us++;
}
void furi_crash(const char* message) {
    (void)message;
}

/* ------------------------------------------------------------------------- */
/* Test helpers                                                              */
/* ------------------------------------------------------------------------- */

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if(!(cond)) {                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            g_failures++;                                                    \
        }                                                                    \
    } while(0)

static void reset(void) {
    g_tx_count = 0;
    g_delay_ms = 0;
    g_delay_us = 0;
}

static bool tx_is(int index, uint8_t address, const uint8_t* data, size_t size) {
    if(index < 0 || index >= g_tx_count) return false;
    const RecordedTx* tx = &g_tx[index];
    return tx->address == address && tx->size == size && memcmp(tx->data, data, size) == 0;
}

static bool tx_cmd(int index, uint8_t command) {
    const uint8_t expected[2] = {0x00, command};
    return tx_is(index, ST7567_I2C_ADDRESS, expected, sizeof(expected));
}

static bool tx_cmd2(int index, uint8_t command, uint8_t arg) {
    const uint8_t expected[3] = {0x00, command, arg};
    return tx_is(index, ST7567_I2C_ADDRESS, expected, sizeof(expected));
}

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

static void test_probe(void) {
    reset();
    CHECK(st7567_i2c_probe());
    CHECK(g_tx_count == 1);
    CHECK(tx_cmd(0, 0xE3)); /* NOP, no state change */
    CHECK(g_tx[0].address == 0x3F);
}

static void test_init_sequence(void) {
    reset();
    st7567_i2c_init();

    /* 0xE2 reset, 0xA2 bias, 0xA0 seg, 0xC8 com, 0x40 start line, 0x25 ratio,
     * 0x81 0x28 contrast, 0x2C, 0x2E, 0x2F power, 0xA4, 0xA6, 0xAF. */
    CHECK(tx_cmd(0, 0xE2));
    CHECK(tx_cmd(1, 0xA2));
    CHECK(tx_cmd(2, 0xA0));
    CHECK(tx_cmd(3, 0xC8));
    CHECK(tx_cmd(4, 0x40));
    CHECK(tx_cmd(5, 0x25));
    CHECK(tx_cmd2(6, 0x81, 0x28));
    CHECK(tx_cmd(7, 0x2C));
    CHECK(tx_cmd(8, 0x2E));
    CHECK(tx_cmd(9, 0x2F));
    CHECK(tx_cmd(10, 0xA4));
    CHECK(tx_cmd(11, 0xA6));
    CHECK(tx_cmd(12, 0xAF));
    CHECK(g_tx_count == 13);

    /* Charge-pump stages must be separated by delays. */
    CHECK(g_delay_ms >= 4);
}

static void test_write_page(void) {
    reset();
    uint8_t payload[16];
    for(int i = 0; i < 16; i++) payload[i] = (uint8_t)(0x80 + i);

    st7567_i2c_write_page(3, 5, payload, sizeof(payload));

    /* page 3 -> 0xB3, column 5 -> 0x10 | 0, 0x00 | 5 */
    CHECK(tx_cmd(0, 0xB3));
    CHECK(tx_cmd(1, 0x10));
    CHECK(tx_cmd(2, 0x05));

    const uint8_t expected[17] = {0x40, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
                                  0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F};
    CHECK(tx_is(3, ST7567_I2C_ADDRESS, expected, sizeof(expected)));
    CHECK(g_tx_count == 4);
}

static void test_write_page_clips_at_right_edge(void) {
    reset();
    uint8_t payload[32];
    memset(payload, 0x5A, sizeof(payload));

    /* column 120 + 32 requested -> only 8 bytes may be sent */
    st7567_i2c_write_page(0, 120, payload, sizeof(payload));
    CHECK(g_tx_count == 4);
    CHECK(g_tx[3].size == 1 + 8);
    CHECK(g_tx[3].data[0] == 0x40);
}

static void test_write_page_rejects_bad_page(void) {
    reset();
    uint8_t byte = 1;
    st7567_i2c_write_page(8, 0, &byte, 1);
    CHECK(g_tx_count == 0);
}

static void test_control_helpers(void) {
    reset();
    st7567_i2c_set_contrast(0x11);
    CHECK(g_tx_count == 1);
    CHECK(tx_cmd2(0, 0x81, 0x11));

    reset();
    st7567_i2c_set_regulation_ratio(0x26);
    CHECK(tx_cmd(0, 0x26));

    reset();
    st7567_i2c_set_display_enabled(true);
    CHECK(tx_cmd(0, 0xAF));

    reset();
    st7567_i2c_set_display_enabled(false);
    CHECK(tx_cmd(0, 0xAE));

    reset();
    st7567_i2c_set_inverted(true);
    CHECK(tx_cmd(0, 0xA7));

    reset();
    st7567_i2c_set_flipped(true);
    CHECK(tx_cmd(0, 0xA1));
    CHECK(tx_cmd(1, 0xC0));
}

static void test_fill_frame_size(void) {
    reset();
    st7567_i2c_fill(0xAA);

    /* 8 pages x (3 command tx + 1 data tx) */
    CHECK(g_tx_count == 8 * 4);
    for(int page = 0; page < 8; page++) {
        CHECK(tx_cmd(page * 4 + 0, (uint8_t)(0xB0 | page)));
        CHECK(g_tx[page * 4 + 3].size == 1 + 128);
        CHECK(g_tx[page * 4 + 3].data[0] == 0x40);
        for(int i = 1; i <= 128; i++) {
            if(g_tx[page * 4 + 3].data[i] != 0xAA) {
                CHECK(0 && "fill payload mismatch");
                break;
            }
        }
    }
}

int main(void) {
    test_probe();
    test_init_sequence();
    test_write_page();
    test_write_page_clips_at_right_edge();
    test_write_page_rejects_bad_page();
    test_control_helpers();
    test_fill_frame_size();

    if(g_failures) {
        fprintf(stderr, "%d display driver check(s) failed\n", g_failures);
        return 1;
    }
    printf("display driver behaviour tests passed\n");
    return 0;
}
