/*
 * Behavioural host test for st7567_i2c.c.
 *
 * Replaces the Furi HAL I2C calls with a recorder and checks the exact byte
 * stream the GM12864-59N would see: address detection across 0x3C..0x3F,
 * control bytes 0x00/0x40, the documented init sequence, page/column
 * addressing, clipping, contrast/config variants and fill.
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
    bool acked;
} RecordedTx;

static RecordedTx g_tx[512];
static int g_tx_count = 0;
static int g_delay_ms = 0;

/* Bit 0 = 0x3C, bit 1 = 0x3D, bit 2 = 0x3E, bit 3 = 0x3F. */
static unsigned g_ack_mask = 1u << (ST7567_I2C_ADDRESS - ST7567_I2C_ADDR_MIN);

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

    if(address < ST7567_I2C_ADDR_MIN || address > ST7567_I2C_ADDR_MAX) return false;
    bool acked = (g_ack_mask & (1u << (address - ST7567_I2C_ADDR_MIN))) != 0;

    if(g_tx_count >= (int)(sizeof(g_tx) / sizeof(g_tx[0]))) return false;
    if(size > sizeof(g_tx[0].data)) return false;
    g_tx[g_tx_count].address = address;
    g_tx[g_tx_count].size = size;
    g_tx[g_tx_count].acked = acked;
    memcpy(g_tx[g_tx_count].data, data, size);
    g_tx_count++;
    return acked;
}

void furi_delay_ms(uint32_t ms) {
    (void)ms;
    g_delay_ms++;
}
void furi_delay_us(uint32_t us) {
    (void)us;
}
void furi_crash(const char* message) {
    (void)message;
}

/* ------------------------------------------------------------------------- */
/* Test helpers                                                              */
/* ------------------------------------------------------------------------- */

static int g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if(!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                   \
        }                                                                   \
    } while(0)

static void reset(void) {
    g_tx_count = 0;
    g_delay_ms = 0;
    st7567_i2c_set_address(0); /* re-enable auto-detect between tests */
}

static bool tx_is(int index, uint8_t address, const uint8_t* data, size_t size) {
    if(index < 0 || index >= g_tx_count) return false;
    const RecordedTx* tx = &g_tx[index];
    return tx->acked && tx->address == address && tx->size == size &&
           memcmp(tx->data, data, size) == 0;
}

static bool tx_cmd_at(int index, uint8_t address, uint8_t command) {
    const uint8_t expected[2] = {0x00, command};
    return tx_is(index, address, expected, sizeof(expected));
}

static bool tx_cmd2_at(int index, uint8_t address, uint8_t command, uint8_t arg) {
    const uint8_t expected[3] = {0x00, command, arg};
    return tx_is(index, address, expected, sizeof(expected));
}

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

static void test_probe(void) {
    reset();
    g_ack_mask = 1u << (0x3F - ST7567_I2C_ADDR_MIN);
    CHECK(st7567_i2c_probe_addr(0x3F));
    CHECK(!st7567_i2c_probe_addr(0x3E));
    CHECK(g_tx_count == 2); /* both attempts are visible on the bus */
    CHECK(tx_cmd_at(0, 0x3F, 0xE3)); /* NOP, no state change */
}

static void test_init_autodetect_3f(void) {
    reset();
    g_ack_mask = 1u << (0x3F - ST7567_I2C_ADDR_MIN);

    CHECK(st7567_i2c_init());
    CHECK(st7567_i2c_get_address() == 0x3F);

    /* scan probes 0x3C..0x3F (4 transactions), then the init sequence */
    CHECK(g_tx_count == 4 + 14);
    CHECK(g_tx[0].address == 0x3C);
    CHECK(g_tx[3].address == 0x3F);

    const uint8_t a = 0x3F;
    CHECK(tx_cmd_at(4, a, 0xE2)); /* reset */
    CHECK(tx_cmd_at(5, a, 0xA2)); /* 1/9 bias */
    CHECK(tx_cmd_at(6, a, 0xA0)); /* SEG normal */
    CHECK(tx_cmd_at(7, a, 0xC8)); /* COM reverse */
    CHECK(tx_cmd_at(8, a, 0x40)); /* start line */
    CHECK(tx_cmd_at(9, a, 0x25)); /* regulation ratio */
    CHECK(tx_cmd2_at(10, a, 0x81, 0x28)); /* contrast */
    CHECK(tx_cmd2_at(11, a, 0xF8, 0x00)); /* booster 4x */
    CHECK(tx_cmd_at(12, a, 0x2C));
    CHECK(tx_cmd_at(13, a, 0x2E));
    CHECK(tx_cmd_at(14, a, 0x2F));
    CHECK(tx_cmd_at(15, a, 0xA4));
    CHECK(tx_cmd_at(16, a, 0xA6));
    CHECK(tx_cmd_at(17, a, 0xAF));
    CHECK(g_delay_ms >= 4);
}

static void test_init_autodetect_3e(void) {
    reset();
    g_ack_mask = 1u << (0x3E - ST7567_I2C_ADDR_MIN);

    CHECK(st7567_i2c_init());
    CHECK(st7567_i2c_get_address() == 0x3E);
    CHECK(g_tx_count == 4 + 14);

    /* every init transaction goes to the detected address */
    for(int i = 4; i < g_tx_count; i++) CHECK(g_tx[i].address == 0x3E);
    CHECK(tx_cmd_at(4, 0x3E, 0xE2));
}

static void test_init_no_device(void) {
    reset();
    g_ack_mask = 0;
    CHECK(!st7567_i2c_init());
    CHECK(g_tx_count == 4); /* only the four probes */
    CHECK(st7567_i2c_get_address() == 0);
}

static void test_init_ex_variant(void) {
    reset();
    g_ack_mask = 1u << (0x3D - ST7567_I2C_ADDR_MIN);

    St7567Config config;
    st7567_i2c_default_config(&config);
    config.contrast = 0x30;
    config.regulation_ratio = 0x26;
    config.bias_1_7 = true;
    config.booster_5x = true;
    config.flipped = true;

    CHECK(st7567_i2c_init_ex(&config));
    CHECK(st7567_i2c_get_address() == 0x3D);

    const uint8_t a = 0x3D;
    CHECK(tx_cmd_at(4, a, 0xE2));
    CHECK(tx_cmd_at(5, a, 0xA3)); /* 1/7 bias */
    CHECK(tx_cmd_at(6, a, 0xA1)); /* SEG reverse */
    CHECK(tx_cmd_at(7, a, 0xC0)); /* COM normal */
    CHECK(tx_cmd_at(9, a, 0x26)); /* ratio */
    CHECK(tx_cmd2_at(10, a, 0x81, 0x30));
    CHECK(tx_cmd2_at(11, a, 0xF8, 0x01)); /* booster 5x */
}

static void test_init_fixed_address_nack(void) {
    reset();
    g_ack_mask = 1u << (0x3F - ST7567_I2C_ADDR_MIN);

    St7567Config config;
    st7567_i2c_default_config(&config);
    config.address = 0x3E; /* not present */

    CHECK(!st7567_i2c_init_ex(&config));
    CHECK(st7567_i2c_get_address() == 0);
}

static void test_write_page(void) {
    reset();
    st7567_i2c_set_address(0x3F); /* no probing needed for a write */

    uint8_t payload[16];
    for(int i = 0; i < 16; i++) payload[i] = (uint8_t)(0x80 + i);

    st7567_i2c_write_page(3, 5, payload, sizeof(payload));

    CHECK(tx_cmd_at(0, 0x3F, 0xB3));
    CHECK(tx_cmd_at(1, 0x3F, 0x10));
    CHECK(tx_cmd_at(2, 0x3F, 0x05));

    const uint8_t expected[17] = {0x40, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
                                  0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F};
    CHECK(tx_is(3, 0x3F, expected, sizeof(expected)));
    CHECK(g_tx_count == 4);
}

static void test_write_page_clips_at_right_edge(void) {
    reset();
    st7567_i2c_set_address(0x3F);

    uint8_t payload[32];
    memset(payload, 0x5A, sizeof(payload));

    st7567_i2c_write_page(0, 120, payload, sizeof(payload));
    CHECK(g_tx_count == 4);
    CHECK(g_tx[3].size == 1 + 8);
    CHECK(g_tx[3].data[0] == 0x40);
}

static void test_write_page_rejects_bad_page(void) {
    reset();
    st7567_i2c_set_address(0x3F);

    uint8_t byte = 1;
    st7567_i2c_write_page(8, 0, &byte, 1);
    CHECK(g_tx_count == 0);
}

static void test_control_helpers(void) {
    reset();
    st7567_i2c_set_address(0x3F);

    st7567_i2c_set_contrast(0x11);
    CHECK(g_tx_count == 1);
    CHECK(tx_cmd2_at(0, 0x3F, 0x81, 0x11));

    reset();
    st7567_i2c_set_address(0x3F);
    st7567_i2c_set_regulation_ratio(0x26);
    CHECK(tx_cmd_at(0, 0x3F, 0x26));

    reset();
    st7567_i2c_set_address(0x3F);
    st7567_i2c_set_display_enabled(true);
    CHECK(tx_cmd_at(0, 0x3F, 0xAF));

    reset();
    st7567_i2c_set_address(0x3F);
    st7567_i2c_set_display_enabled(false);
    CHECK(tx_cmd_at(0, 0x3F, 0xAE));

    reset();
    st7567_i2c_set_address(0x3F);
    st7567_i2c_set_inverted(true);
    CHECK(tx_cmd_at(0, 0x3F, 0xA7));

    reset();
    st7567_i2c_set_address(0x3F);
    st7567_i2c_set_flipped(true);
    CHECK(tx_cmd_at(0, 0x3F, 0xA1));
    CHECK(tx_cmd_at(1, 0x3F, 0xC0));
}

static void test_fill_frame_size(void) {
    reset();
    st7567_i2c_set_address(0x3F);

    st7567_i2c_fill(0xAA);

    CHECK(g_tx_count == 8 * 4);
    for(int page = 0; page < 8; page++) {
        CHECK(tx_cmd_at(page * 4 + 0, 0x3F, (uint8_t)(0xB0 | page)));
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
    test_init_autodetect_3f();
    test_init_autodetect_3e();
    test_init_no_device();
    test_init_ex_variant();
    test_init_fixed_address_nack();
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
