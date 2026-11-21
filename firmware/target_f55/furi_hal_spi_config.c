/**
 * @file furi_hal_spi_config.c
 * @brief SPI bus layout for the "f55" target.
 *
 * SPI1 (AF5): PA5 SCK / PB4 MISO / PB5 MOSI  -> CC1101 (CS PD0), header
 * SPI2 (AF5): PD1 SCK / PB15 MOSI / PC2 MISO -> microSD (CS PC12), nRF24 (CSN PB0)
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include <furi_hal_spi_config.h>
#include <furi_hal_resources.h>
#include <furi_hal_spi.h>
#include <furi_hal_bus.h>
#include <furi.h>

#define TAG "FuriHalSpiConfig"

/* SPI Presets */

const LL_SPI_InitTypeDef furi_hal_spi_preset_2edge_low_8m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_2EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_8m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_4m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_16m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV2,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_2m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV32,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

/* SPI Buses */

FuriMutex* furi_hal_spi_bus_r_mutex = NULL;

void furi_hal_spi_config_init_early(void) {
    /* SPI2 is initialised early because the display used to be there; keep the
     * same ordering so Sub-GHz/SD ordering stays unchanged. */
    furi_hal_spi_bus_init(&furi_hal_spi_bus_d);
}

void furi_hal_spi_config_deinit_early(void) {
    furi_hal_spi_bus_deinit(&furi_hal_spi_bus_d);
}

void furi_hal_spi_config_init(void) {
    furi_hal_spi_bus_init(&furi_hal_spi_bus_r);

    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_nrf24);
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_sd_fast);
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_sd_slow);

    FURI_LOG_I(TAG, "Init OK");
}

static void furi_hal_spi_bus_r_event_callback(FuriHalSpiBus* bus, FuriHalSpiBusEvent event) {
    if(event == FuriHalSpiBusEventInit) {
        furi_hal_spi_bus_r_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        bus->current_handle = NULL;
    } else if(event == FuriHalSpiBusEventDeinit) {
        furi_mutex_free(furi_hal_spi_bus_r_mutex);
    } else if(event == FuriHalSpiBusEventLock) {
        furi_check(furi_mutex_acquire(furi_hal_spi_bus_r_mutex, FuriWaitForever) == FuriStatusOk);
    } else if(event == FuriHalSpiBusEventUnlock) {
        furi_check(furi_mutex_release(furi_hal_spi_bus_r_mutex) == FuriStatusOk);
    } else if(event == FuriHalSpiBusEventActivate) {
        furi_hal_bus_enable(FuriHalBusSPI1);
    } else if(event == FuriHalSpiBusEventDeactivate) {
        furi_hal_bus_disable(FuriHalBusSPI1);
    }
}

FuriHalSpiBus furi_hal_spi_bus_r = {
    .spi = SPI1,
    .callback = furi_hal_spi_bus_r_event_callback,
};

FuriMutex* furi_hal_spi_bus_d_mutex = NULL;

static void furi_hal_spi_bus_d_event_callback(FuriHalSpiBus* bus, FuriHalSpiBusEvent event) {
    if(event == FuriHalSpiBusEventInit) {
        furi_hal_spi_bus_d_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        bus->current_handle = NULL;
    } else if(event == FuriHalSpiBusEventDeinit) {
        furi_mutex_free(furi_hal_spi_bus_d_mutex);
    } else if(event == FuriHalSpiBusEventLock) {
        furi_check(furi_mutex_acquire(furi_hal_spi_bus_d_mutex, FuriWaitForever) == FuriStatusOk);
    } else if(event == FuriHalSpiBusEventUnlock) {
        furi_check(furi_mutex_release(furi_hal_spi_bus_d_mutex) == FuriStatusOk);
    } else if(event == FuriHalSpiBusEventActivate) {
        furi_hal_bus_enable(FuriHalBusSPI2);
    } else if(event == FuriHalSpiBusEventDeactivate) {
        furi_hal_bus_disable(FuriHalBusSPI2);
    }
}

FuriHalSpiBus furi_hal_spi_bus_d = {
    .spi = SPI2,
    .callback = furi_hal_spi_bus_d_event_callback,
};

/* SPI1 handle callbacks */

inline static void furi_hal_spi_bus_r_handle_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event,
    const LL_SPI_InitTypeDef* preset) {
    if(event == FuriHalSpiBusHandleEventInit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    } else if(event == FuriHalSpiBusHandleEventDeinit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    } else if(event == FuriHalSpiBusHandleEventActivate) {
        LL_SPI_Init(handle->bus->spi, (LL_SPI_InitTypeDef*)preset);
        LL_SPI_SetRxFIFOThreshold(handle->bus->spi, LL_SPI_RX_FIFO_TH_QUARTER);
        LL_SPI_Enable(handle->bus->spi);

        furi_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furi_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furi_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);

        furi_hal_gpio_write(handle->cs, false);
    } else if(event == FuriHalSpiBusHandleEventDeactivate) {
        furi_hal_gpio_write(handle->cs, true);

        furi_hal_gpio_init(handle->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(handle->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(handle->sck, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        LL_SPI_Disable(handle->bus->spi);
    }
}

static void furi_hal_spi_bus_handle_subghz_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event) {
    furi_hal_spi_bus_r_handle_event_callback(handle, event, &furi_hal_spi_preset_1edge_low_8m);
}

const FuriHalSpiBusHandle furi_hal_spi_bus_handle_subghz = {
    .bus = &furi_hal_spi_bus_r,
    .callback = furi_hal_spi_bus_handle_subghz_event_callback,
    .miso = &gpio_spi_r_miso,
    .mosi = &gpio_spi_r_mosi,
    .sck = &gpio_spi_r_sck,
    .cs = &gpio_subghz_cs,
};

static void furi_hal_spi_bus_handle_external_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event) {
    furi_hal_spi_bus_r_handle_event_callback(handle, event, &furi_hal_spi_preset_1edge_low_2m);
}

const FuriHalSpiBusHandle furi_hal_spi_bus_handle_external = {
    .bus = &furi_hal_spi_bus_r,
    .callback = furi_hal_spi_bus_handle_external_event_callback,
    .miso = &gpio_ext_pa6,
    .mosi = &gpio_ext_pa7,
    .sck = &gpio_ext_pb3,
    .cs = &gpio_ext_pa4,
};

/* SPI2 handle callbacks */

inline static void furi_hal_spi_bus_d_handle_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event,
    const LL_SPI_InitTypeDef* preset) {
    if(event == FuriHalSpiBusHandleEventInit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullUp, GpioSpeedVeryHigh);

        furi_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI2);
        furi_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI2);
        furi_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI2);
    } else if(event == FuriHalSpiBusHandleEventDeinit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeAnalog, GpioPullUp, GpioSpeedLow);
    } else if(event == FuriHalSpiBusHandleEventActivate) {
        LL_SPI_Init(handle->bus->spi, (LL_SPI_InitTypeDef*)preset);
        LL_SPI_SetRxFIFOThreshold(handle->bus->spi, LL_SPI_RX_FIFO_TH_QUARTER);
        LL_SPI_Enable(handle->bus->spi);
        furi_hal_gpio_write(handle->cs, false);
    } else if(event == FuriHalSpiBusHandleEventDeactivate) {
        furi_hal_gpio_write(handle->cs, true);
        LL_SPI_Disable(handle->bus->spi);
    }
}

static void furi_hal_spi_bus_handle_nrf24_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event) {
    /* nRF24L01+ is SPI mode 0; 4 MHz is conservative on the shared SPI2 bus. */
    furi_hal_spi_bus_d_handle_event_callback(handle, event, &furi_hal_spi_preset_1edge_low_4m);
}

const FuriHalSpiBusHandle furi_hal_spi_bus_handle_nrf24 = {
    .bus = &furi_hal_spi_bus_d,
    .callback = furi_hal_spi_bus_handle_nrf24_event_callback,
    .miso = &gpio_spi_d_miso,
    .mosi = &gpio_spi_d_mosi,
    .sck = &gpio_spi_d_sck,
    .cs = &gpio_nrf24_csn,
};

static void furi_hal_spi_bus_handle_sd_fast_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event) {
    furi_hal_spi_bus_d_handle_event_callback(handle, event, &furi_hal_spi_preset_1edge_low_16m);
}

const FuriHalSpiBusHandle furi_hal_spi_bus_handle_sd_fast = {
    .bus = &furi_hal_spi_bus_d,
    .callback = furi_hal_spi_bus_handle_sd_fast_event_callback,
    .miso = &gpio_spi_d_miso,
    .mosi = &gpio_spi_d_mosi,
    .sck = &gpio_spi_d_sck,
    .cs = &gpio_sdcard_cs,
};

static void furi_hal_spi_bus_handle_sd_slow_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event) {
    furi_hal_spi_bus_d_handle_event_callback(handle, event, &furi_hal_spi_preset_1edge_low_2m);
}

const FuriHalSpiBusHandle furi_hal_spi_bus_handle_sd_slow = {
    .bus = &furi_hal_spi_bus_d,
    .callback = furi_hal_spi_bus_handle_sd_slow_event_callback,
    .miso = &gpio_spi_d_miso,
    .mosi = &gpio_spi_d_mosi,
    .sck = &gpio_spi_d_sck,
    .cs = &gpio_sdcard_cs,
};
