#pragma once

/**
 * @file furi_hal_resources.h
 * @brief Board pin map for the "f55" WB55 custom clone target.
 *
 * Derived from Flipper Zero's targets/f7 map with three changes:
 *   - the LCD moved to I2C1 (PA9/PA10), so its SPI pins are gone;
 *   - PB0/PC11/PB1 are now nRF24 CSN/CE/IRQ;
 *   - NFC, 125 kHz RFID and iButton are not populated.
 *
 * Every alternate function used here is listed in hardware/PINOUT_AND_WIRING.md
 * and was verified against ST DS11929 Rev 4, Table 17.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include <furi.h>
#include <furi_hal_adc.h>
#include <furi_hal_pwm.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Input Related Constants */
#define INPUT_DEBOUNCE_TICKS 4

/* Input Keys */
typedef enum {
    InputKeyUp,
    InputKeyDown,
    InputKeyRight,
    InputKeyLeft,
    InputKeyOk,
    InputKeyBack,
    InputKeyMAX, /**< Special value */
} InputKey;

/* Light */
typedef enum {
    LightRed = (1 << 0),
    LightGreen = (1 << 1),
    LightBlue = (1 << 2),
    LightBacklight = (1 << 3),
} Light;

typedef struct {
    const GpioPin* gpio;
    const InputKey key;
    const bool inverted;
    const char* name;
} InputPin;

typedef struct {
    const GpioPin* pin;
    const char* name;
    const FuriHalAdcChannel channel;
    const FuriHalPwmOutputId pwm_output;
    const uint8_t number;
    const bool debug;
} GpioPinRecord;

extern const InputPin input_pins[];
extern const size_t input_pins_count;

extern const GpioPinRecord gpio_pins[];
extern const size_t gpio_pins_count;

/* Debug */
extern const GpioPin gpio_swdio;
extern const GpioPin gpio_swclk;

/* On-board peripherals */
extern const GpioPin gpio_vibro;
extern const GpioPin gpio_speaker;

extern const GpioPin gpio_cc1101_g0;
extern const GpioPin gpio_rf_sw_0;
extern const GpioPin gpio_subghz_cs;

extern const GpioPin gpio_nrf24_csn;
extern const GpioPin gpio_nrf24_ce;
extern const GpioPin gpio_nrf24_irq;

extern const GpioPin gpio_sdcard_cs;
extern const GpioPin gpio_sdcard_cd;

extern const GpioPin gpio_infrared_rx;
extern const GpioPin gpio_infrared_tx;

/* User interface */
extern const GpioPin gpio_button_up;
extern const GpioPin gpio_button_down;
extern const GpioPin gpio_button_right;
extern const GpioPin gpio_button_left;
extern const GpioPin gpio_button_ok;
extern const GpioPin gpio_button_back;

/* Buses */
extern const GpioPin gpio_spi_d_miso;
extern const GpioPin gpio_spi_d_mosi;
extern const GpioPin gpio_spi_d_sck;
extern const GpioPin gpio_spi_r_miso;
extern const GpioPin gpio_spi_r_mosi;
extern const GpioPin gpio_spi_r_sck;

/* External header (kept compatible with Flipper's numbering where possible) */
extern const GpioPin gpio_ext_pc0;
extern const GpioPin gpio_ext_pc1;
extern const GpioPin gpio_ext_pc3;
extern const GpioPin gpio_ext_pb2;
extern const GpioPin gpio_ext_pb3;
extern const GpioPin gpio_ext_pa4;
extern const GpioPin gpio_ext_pa6;
extern const GpioPin gpio_ext_pa7;
extern const GpioPin gpio_ext_pb14;

/* Companion / host */
extern const GpioPin gpio_usart_tx;
extern const GpioPin gpio_usart_rx;

/* Display bus (the stock "power" I2C1 handle) */
extern const GpioPin gpio_i2c_power_sda;
extern const GpioPin gpio_i2c_power_scl;

/* Power */
extern const GpioPin gpio_periph_power;
extern const GpioPin gpio_usb_dm;
extern const GpioPin gpio_usb_dp;

void furi_hal_resources_init_early(void);
void furi_hal_resources_deinit_early(void);
void furi_hal_resources_init(void);

int32_t furi_hal_resources_get_ext_pin_number(const GpioPin* gpio);
const GpioPinRecord* furi_hal_resources_pin_by_name(const char* name);
const GpioPinRecord* furi_hal_resources_pin_by_number(uint8_t number);

#ifdef __cplusplus
}
#endif
