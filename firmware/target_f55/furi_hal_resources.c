/**
 * @file furi_hal_resources.c
 * @brief Board pin map for the "f55" WB55 custom clone target.
 *
 * See hardware/PINOUT_AND_WIRING.md for the authoritative table and for the
 * DS11929 alternate-function verification of every peripheral pin.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include <furi_hal_resources.h>
#include <furi_hal_bus.h>
#include <furi.h>

#include <stm32wbxx_ll_pwr.h>

#define TAG "FuriHalResources"

/* Debug */
const GpioPin gpio_swdio = {.port = GPIOA, .pin = LL_GPIO_PIN_13};
const GpioPin gpio_swclk = {.port = GPIOA, .pin = LL_GPIO_PIN_14};

/* On-board peripherals */
const GpioPin gpio_vibro = {.port = GPIOA, .pin = LL_GPIO_PIN_8};
const GpioPin gpio_speaker = {.port = GPIOB, .pin = LL_GPIO_PIN_8};

const GpioPin gpio_cc1101_g0 = {.port = GPIOA, .pin = LL_GPIO_PIN_1};
const GpioPin gpio_rf_sw_0 = {.port = GPIOC, .pin = LL_GPIO_PIN_4};
const GpioPin gpio_subghz_cs = {.port = GPIOD, .pin = LL_GPIO_PIN_0};

/* nRF24L01+ on the SPI2 pins freed by the I2C display */
const GpioPin gpio_nrf24_csn = {.port = GPIOB, .pin = LL_GPIO_PIN_0};
const GpioPin gpio_nrf24_ce = {.port = GPIOC, .pin = LL_GPIO_PIN_11};
const GpioPin gpio_nrf24_irq = {.port = GPIOB, .pin = LL_GPIO_PIN_1};

/* Storage */
const GpioPin gpio_sdcard_cs = {.port = GPIOC, .pin = LL_GPIO_PIN_12};
const GpioPin gpio_sdcard_cd = {.port = GPIOC, .pin = LL_GPIO_PIN_10};

/* Infrared */
const GpioPin gpio_infrared_rx = {.port = GPIOA, .pin = LL_GPIO_PIN_0};
const GpioPin gpio_infrared_tx = {.port = GPIOB, .pin = LL_GPIO_PIN_9};

/* User interface */
const GpioPin gpio_button_up = {.port = GPIOB, .pin = LL_GPIO_PIN_10};
const GpioPin gpio_button_down = {.port = GPIOC, .pin = LL_GPIO_PIN_6};
const GpioPin gpio_button_right = {.port = GPIOB, .pin = LL_GPIO_PIN_12};
const GpioPin gpio_button_left = {.port = GPIOB, .pin = LL_GPIO_PIN_11};
const GpioPin gpio_button_ok = {.port = GPIOH, .pin = LL_GPIO_PIN_3};
const GpioPin gpio_button_back = {.port = GPIOC, .pin = LL_GPIO_PIN_13};

/* Buses */
const GpioPin gpio_spi_d_miso = {.port = GPIOC, .pin = LL_GPIO_PIN_2};
const GpioPin gpio_spi_d_mosi = {.port = GPIOB, .pin = LL_GPIO_PIN_15};
const GpioPin gpio_spi_d_sck = {.port = GPIOD, .pin = LL_GPIO_PIN_1};

const GpioPin gpio_spi_r_miso = {.port = GPIOB, .pin = LL_GPIO_PIN_4};
const GpioPin gpio_spi_r_mosi = {.port = GPIOB, .pin = LL_GPIO_PIN_5};
const GpioPin gpio_spi_r_sck = {.port = GPIOA, .pin = LL_GPIO_PIN_5};

/* External header */
const GpioPin gpio_ext_pc0 = {.port = GPIOC, .pin = LL_GPIO_PIN_0};
const GpioPin gpio_ext_pc1 = {.port = GPIOC, .pin = LL_GPIO_PIN_1};
const GpioPin gpio_ext_pc3 = {.port = GPIOC, .pin = LL_GPIO_PIN_3};
const GpioPin gpio_ext_pb2 = {.port = GPIOB, .pin = LL_GPIO_PIN_2};
const GpioPin gpio_ext_pb3 = {.port = GPIOB, .pin = LL_GPIO_PIN_3};
const GpioPin gpio_ext_pa4 = {.port = GPIOA, .pin = LL_GPIO_PIN_4};
const GpioPin gpio_ext_pa6 = {.port = GPIOA, .pin = LL_GPIO_PIN_6};
const GpioPin gpio_ext_pa7 = {.port = GPIOA, .pin = LL_GPIO_PIN_7};
const GpioPin gpio_ext_pb14 = {.port = GPIOB, .pin = LL_GPIO_PIN_14};

/* Companion / host */
const GpioPin gpio_usart_tx = {.port = GPIOB, .pin = LL_GPIO_PIN_6};
const GpioPin gpio_usart_rx = {.port = GPIOB, .pin = LL_GPIO_PIN_7};

/* Display bus: the stock "power" I2C1 handle, now carrying the LCD */
const GpioPin gpio_i2c_power_sda = {.port = GPIOA, .pin = LL_GPIO_PIN_10};
const GpioPin gpio_i2c_power_scl = {.port = GPIOA, .pin = LL_GPIO_PIN_9};

/* Power */
const GpioPin gpio_periph_power = {.port = GPIOA, .pin = LL_GPIO_PIN_3};
const GpioPin gpio_usb_dm = {.port = GPIOA, .pin = LL_GPIO_PIN_11};
const GpioPin gpio_usb_dp = {.port = GPIOA, .pin = LL_GPIO_PIN_12};

const GpioPinRecord gpio_pins[] = {
    // 5V: 1
    {.pin = &gpio_ext_pa7,
     .name = "PA7",
     .channel = FuriHalAdcChannel12,
     .pwm_output = FuriHalPwmOutputIdTim1PA7,
     .number = 2,
     .debug = false},
    {.pin = &gpio_ext_pa6,
     .name = "PA6",
     .channel = FuriHalAdcChannel11,
     .number = 3,
     .debug = false},
    {.pin = &gpio_ext_pa4,
     .name = "PA4",
     .channel = FuriHalAdcChannel9,
     .pwm_output = FuriHalPwmOutputIdLptim2PA4,
     .number = 4,
     .debug = false},
    {.pin = &gpio_ext_pb3,
     .name = "PB3",
     .channel = FuriHalAdcChannelNone,
     .number = 5,
     .debug = false},
    {.pin = &gpio_ext_pb2,
     .name = "PB2",
     .channel = FuriHalAdcChannelNone,
     .number = 6,
     .debug = false},
    {.pin = &gpio_ext_pc3,
     .name = "PC3",
     .channel = FuriHalAdcChannel4,
     .number = 7,
     .debug = false},
    // GND: 8
    // 3v3: 9
    {.pin = &gpio_swclk,
     .name = "PA14",
     .channel = FuriHalAdcChannelNone,
     .number = 10,
     .debug = true},
    // GND: 11
    {.pin = &gpio_swdio,
     .name = "PA13",
     .channel = FuriHalAdcChannelNone,
     .number = 12,
     .debug = true},
    {.pin = &gpio_usart_tx,
     .name = "PB6",
     .channel = FuriHalAdcChannelNone,
     .number = 13,
     .debug = true},
    {.pin = &gpio_usart_rx,
     .name = "PB7",
     .channel = FuriHalAdcChannelNone,
     .number = 14,
     .debug = true},
    {.pin = &gpio_ext_pc1,
     .name = "PC1",
     .channel = FuriHalAdcChannel2,
     .number = 15,
     .debug = false},
    {.pin = &gpio_ext_pc0,
     .name = "PC0",
     .channel = FuriHalAdcChannel1,
     .number = 16,
     .debug = false},
    // Pin 17 is free in stock (iButton on PB14). Keep PB14 as a plain GPIO so
    // the external header numbering still works; there is no iButton driver.
    {.pin = &gpio_ext_pb14,
     .name = "PB14",
     .channel = FuriHalAdcChannelNone,
     .number = 17,
     .debug = false},
    // GND: 18

    /* Dangerous pins, may damage hardware */
    {.pin = &gpio_speaker,
     .name = "PB8",
     .channel = FuriHalAdcChannelNone,
     .number = 0,
     .debug = true},
    {.pin = &gpio_infrared_tx,
     .name = "PB9",
     .channel = FuriHalAdcChannelNone,
     .number = 0,
     .debug = true},
};

const size_t gpio_pins_count = COUNT_OF(gpio_pins);

const InputPin input_pins[] = {
    {.gpio = &gpio_button_up, .key = InputKeyUp, .inverted = true, .name = "Up"},
    {.gpio = &gpio_button_down, .key = InputKeyDown, .inverted = true, .name = "Down"},
    {.gpio = &gpio_button_right, .key = InputKeyRight, .inverted = true, .name = "Right"},
    {.gpio = &gpio_button_left, .key = InputKeyLeft, .inverted = true, .name = "Left"},
    {.gpio = &gpio_button_ok, .key = InputKeyOk, .inverted = false, .name = "OK"},
    {.gpio = &gpio_button_back, .key = InputKeyBack, .inverted = true, .name = "Back"},
};

const size_t input_pins_count = COUNT_OF(input_pins);

static void furi_hal_resources_init_input_pins(GpioMode mode) {
    for(size_t i = 0; i < input_pins_count; i++) {
        furi_hal_gpio_init(
            input_pins[i].gpio,
            mode,
            (input_pins[i].inverted) ? GpioPullUp : GpioPullDown,
            GpioSpeedLow);
    }
}

static void furi_hal_resources_init_gpio_pins(GpioMode mode) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        if(!gpio_pins[i].debug) {
            furi_hal_gpio_init(gpio_pins[i].pin, mode, GpioPullNo, GpioSpeedLow);
        }
    }
}

void furi_hal_resources_init_early(void) {
    furi_hal_bus_enable(FuriHalBusGPIOA);
    furi_hal_bus_enable(FuriHalBusGPIOB);
    furi_hal_bus_enable(FuriHalBusGPIOC);
    furi_hal_bus_enable(FuriHalBusGPIOD);
    furi_hal_bus_enable(FuriHalBusGPIOE);
    furi_hal_bus_enable(FuriHalBusGPIOH);

    furi_hal_resources_init_input_pins(GpioModeInput);

    // Explicit, surviving reset, pulls
    LL_PWR_EnablePUPDCfg();
    LL_PWR_EnableGPIOPullDown(LL_PWR_GPIO_A, LL_PWR_GPIO_BIT_8); // gpio_vibro
    LL_PWR_EnableGPIOPullDown(LL_PWR_GPIO_B, LL_PWR_GPIO_BIT_8); // gpio_speaker
    LL_PWR_EnableGPIOPullDown(LL_PWR_GPIO_B, LL_PWR_GPIO_BIT_9); // gpio_infrared_tx

    // SD card power switch control (harmless if the switch is not fitted)
    furi_hal_gpio_write(&gpio_periph_power, 1);
    furi_hal_gpio_init(&gpio_periph_power, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);

    // nRF24 safe state: CSN high (deselected), CE low (standby), IRQ pulled up.
    furi_hal_gpio_write(&gpio_nrf24_csn, true);
    furi_hal_gpio_init(&gpio_nrf24_csn, GpioModeOutputPushPull, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_nrf24_ce, false);
    furi_hal_gpio_init(&gpio_nrf24_ce, GpioModeOutputPushPull, GpioPullDown, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_nrf24_irq, GpioModeInput, GpioPullUp, GpioSpeedLow);

    // Hard reset USB
    furi_hal_gpio_write(&gpio_usb_dm, 1);
    furi_hal_gpio_write(&gpio_usb_dp, 1);
    furi_hal_gpio_init_simple(&gpio_usb_dm, GpioModeOutputOpenDrain);
    furi_hal_gpio_init_simple(&gpio_usb_dp, GpioModeOutputOpenDrain);
    furi_hal_gpio_write(&gpio_usb_dm, 0);
    furi_hal_gpio_write(&gpio_usb_dp, 0);
    furi_delay_us(5); // Device Driven disconnect: 2.5us + extra to compensate cables
    furi_hal_gpio_write(&gpio_usb_dm, 1);
    furi_hal_gpio_write(&gpio_usb_dp, 1);
    furi_hal_gpio_init_simple(&gpio_usb_dm, GpioModeAnalog);
    furi_hal_gpio_init_simple(&gpio_usb_dp, GpioModeAnalog);
    furi_hal_gpio_write(&gpio_usb_dm, 0);
    furi_hal_gpio_write(&gpio_usb_dp, 0);

    // External header pins
    furi_hal_resources_init_gpio_pins(GpioModeAnalog);
}

void furi_hal_resources_deinit_early(void) {
    furi_hal_resources_init_input_pins(GpioModeAnalog);
    furi_hal_bus_disable(FuriHalBusGPIOA);
    furi_hal_bus_disable(FuriHalBusGPIOB);
    furi_hal_bus_disable(FuriHalBusGPIOC);
    furi_hal_bus_disable(FuriHalBusGPIOD);
    furi_hal_bus_disable(FuriHalBusGPIOE);
    furi_hal_bus_disable(FuriHalBusGPIOH);
}

void furi_hal_resources_init(void) {
    // Button pins
    furi_hal_resources_init_input_pins(GpioModeInterruptRiseFall);

    // SD card detect: input with pull-up, switch to GND when a card is present
    furi_hal_gpio_init(&gpio_sdcard_cd, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_sdcard_cd, 0);

    // CC1101 antenna switch
    furi_hal_gpio_init(&gpio_rf_sw_0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);

    NVIC_SetPriority(EXTI0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI0_IRQn);

    NVIC_SetPriority(EXTI1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI1_IRQn);

    NVIC_SetPriority(EXTI2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI2_IRQn);

    NVIC_SetPriority(EXTI3_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI3_IRQn);

    NVIC_SetPriority(EXTI4_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI4_IRQn);

    NVIC_SetPriority(EXTI9_5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI9_5_IRQn);

    NVIC_SetPriority(EXTI15_10_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(EXTI15_10_IRQn);

    FURI_LOG_I(TAG, "Init OK");
}

int32_t furi_hal_resources_get_ext_pin_number(const GpioPin* gpio) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        if(gpio_pins[i].pin == gpio) {
            return gpio_pins[i].number;
        }
    }
    return -1;
}

const GpioPinRecord* furi_hal_resources_pin_by_name(const char* name) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        const GpioPinRecord* record = &gpio_pins[i];
        if(strcasecmp(name, record->name) == 0) return record;
    }
    return NULL;
}

const GpioPinRecord* furi_hal_resources_pin_by_number(uint8_t number) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        const GpioPinRecord* record = &gpio_pins[i];
        if(record->number == number) return record;
    }
    return NULL;
}
