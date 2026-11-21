/**
 * @file nrf24.c
 * @brief Minimal nRF24L01+ driver for the "f55" board.
 *
 * CSN is driven by the Furi HAL SPI bus handle, so every helper below brackets
 * its transfer with furi_hal_spi_acquire()/release(). CE and IRQ are plain
 * GPIOs from furi_hal_resources.h.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include "nrf24.h"

#include <furi.h>
#include <furi_hal.h>

#define TAG "Nrf24"

#define SPI_TIMEOUT_MS 100

/* CONFIG bits */
#define CONFIG_PWR_UP    (1u << 1)
#define CONFIG_PRIM_RX   (1u << 0)
#define CONFIG_CRC_EN    (1u << 3)
#define CONFIG_CRC_2BYTE (1u << 2)
#define CONFIG_MASK_RX_DR (1u << 6)
#define CONFIG_MASK_TX_DS (1u << 5)
#define CONFIG_MASK_MAX_RT (1u << 4)

/* RF_SETUP bits */
#define RF_SETUP_RF_DR_LOW  (1u << 5)
#define RF_SETUP_RF_DR_HIGH (1u << 3)
#define RF_SETUP_RF_PWR_SHIFT 1

/* STATUS / FIFO bits */
#define FIFO_STATUS_TX_EMPTY (1u << 4)
#define FIFO_STATUS_RX_EMPTY (1u << 0)

static bool nrf24_xfer(const uint8_t* tx, uint8_t* rx, size_t size) {
    bool ok;

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nrf24);
    ok = furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_nrf24, tx, rx, size, SPI_TIMEOUT_MS);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_nrf24);

    return ok;
}

static uint8_t nrf24_cmd(uint8_t command) {
    uint8_t tx[2] = {command, NRF24_CMD_NOP};
    uint8_t rx[2] = {0};
    nrf24_xfer(tx, rx, sizeof(tx));
    return rx[1];
}

uint8_t nrf24_read_reg(uint8_t reg) {
    return nrf24_cmd((uint8_t)(NRF24_CMD_R_REGISTER | (reg & 0x1F)));
}

void nrf24_write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx[2] = {(uint8_t)(NRF24_CMD_W_REGISTER | (reg & 0x1F)), value};
    uint8_t rx[2];
    nrf24_xfer(tx, rx, sizeof(tx));
}

void nrf24_read_buf(uint8_t reg, uint8_t* data, size_t size) {
    if(size == 0 || size > 32) return;

    uint8_t tx[1 + 32];
    uint8_t rx[1 + 32];
    tx[0] = (uint8_t)(NRF24_CMD_R_REGISTER | (reg & 0x1F));
    memset(&tx[1], NRF24_CMD_NOP, size);
    nrf24_xfer(tx, rx, size + 1);
    memcpy(data, &rx[1], size);
}

void nrf24_write_buf(uint8_t reg, const uint8_t* data, size_t size) {
    if(size == 0 || size > 32) return;

    uint8_t tx[1 + 32];
    uint8_t rx[1 + 32];
    tx[0] = (uint8_t)(NRF24_CMD_W_REGISTER | (reg & 0x1F));
    memcpy(&tx[1], data, size);
    nrf24_xfer(tx, rx, size + 1);
}

uint8_t nrf24_status(void) {
    uint8_t tx[2] = {NRF24_CMD_NOP, NRF24_CMD_NOP};
    uint8_t rx[2] = {0};
    nrf24_xfer(tx, rx, sizeof(tx));
    return rx[1];
}

void nrf24_clear_irq(uint8_t mask) {
    nrf24_write_reg(NRF24_REG_STATUS, mask & 0x70);
}

void nrf24_flush_tx(void) {
    uint8_t tx[1] = {NRF24_CMD_FLUSH_TX};
    uint8_t rx[1];
    nrf24_xfer(tx, rx, 1);
}

void nrf24_flush_rx(void) {
    uint8_t tx[1] = {NRF24_CMD_FLUSH_RX};
    uint8_t rx[1];
    nrf24_xfer(tx, rx, 1);
}

bool nrf24_init(void) {
    uint8_t config = nrf24_read_reg(NRF24_REG_CONFIG);
    if(config == 0x00 || config == 0xFF) {
        FURI_LOG_E(TAG, "not detected (CONFIG=0x%02X)", config);
        return false;
    }

    /* Known-good baseline: CRC on, IRQs unmasked, power down, PTX. */
    nrf24_write_reg(NRF24_REG_CONFIG, CONFIG_CRC_EN | CONFIG_CRC_2BYTE);
    nrf24_write_reg(NRF24_REG_EN_AA, 0x3F); /* auto-ack on pipes 0-5 */
    nrf24_write_reg(NRF24_REG_EN_RXADDR, 0x03); /* pipes 0-1 */
    nrf24_write_reg(NRF24_REG_SETUP_AW, 0x03); /* 5-byte addresses */
    nrf24_write_reg(NRF24_REG_SETUP_RETR, 0x03); /* 250 us, 3 retries */
    nrf24_set_channel(76);
    nrf24_set_rate(Nrf24Rate1M);
    nrf24_set_pa(Nrf24Pa0dBm);
    nrf24_write_reg(NRF24_REG_DYNPD, 0x00);
    nrf24_write_reg(NRF24_REG_FEATURE, 0x00);

    nrf24_flush_tx();
    nrf24_flush_rx();
    nrf24_clear_irq(0x70);

    FURI_LOG_I(TAG, "detected (CONFIG=0x%02X)", nrf24_read_reg(NRF24_REG_CONFIG));
    return true;
}

void nrf24_set_channel(uint8_t channel) {
    nrf24_write_reg(NRF24_REG_RF_CH, channel > 125 ? 125 : channel);
}

void nrf24_set_rate(Nrf24Rate rate) {
    uint8_t rf_setup = nrf24_read_reg(NRF24_REG_RF_SETUP) & ~(RF_SETUP_RF_DR_LOW | RF_SETUP_RF_DR_HIGH);
    if(rate == Nrf24Rate250k) {
        rf_setup |= RF_SETUP_RF_DR_LOW;
    } else if(rate == Nrf24Rate2M) {
        rf_setup |= RF_SETUP_RF_DR_HIGH;
    }
    nrf24_write_reg(NRF24_REG_RF_SETUP, rf_setup);
}

void nrf24_set_pa(Nrf24Pa pa) {
    uint8_t rf_setup = nrf24_read_reg(NRF24_REG_RF_SETUP) & ~(0x03 << RF_SETUP_RF_PWR_SHIFT);
    rf_setup |= (uint8_t)((pa & 0x03) << RF_SETUP_RF_PWR_SHIFT);
    nrf24_write_reg(NRF24_REG_RF_SETUP, rf_setup);
}

void nrf24_set_address_width(uint8_t width) {
    if(width < 3 || width > 5) return;
    nrf24_write_reg(NRF24_REG_SETUP_AW, (uint8_t)(width - 2));
}

void nrf24_set_rx_address(uint8_t pipe, const uint8_t* address, size_t size) {
    if(pipe > 5 || size == 0 || size > 5) return;
    nrf24_write_buf((uint8_t)(NRF24_REG_RX_ADDR_P0 + pipe), address, size);
}

void nrf24_set_tx_address(const uint8_t* address, size_t size) {
    if(size == 0 || size > 5) return;
    nrf24_write_buf(NRF24_REG_TX_ADDR, address, size);
}

void nrf24_set_payload_width(uint8_t pipe, uint8_t width) {
    if(pipe > 5 || width > 32) return;
    nrf24_write_reg((uint8_t)(NRF24_REG_RX_PW_P0 + pipe), width);
}

void nrf24_power_up(bool enabled) {
    uint8_t config = nrf24_read_reg(NRF24_REG_CONFIG);
    config = enabled ? (config | CONFIG_PWR_UP) : (config & ~CONFIG_PWR_UP);
    nrf24_write_reg(NRF24_REG_CONFIG, config);
    furi_delay_ms(2); /* Tpd2stby / Tstby2a */
}

void nrf24_rx_mode(void) {
    uint8_t config = nrf24_read_reg(NRF24_REG_CONFIG);
    nrf24_write_reg(NRF24_REG_CONFIG, (config | CONFIG_PRIM_RX) | CONFIG_PWR_UP);
    furi_hal_gpio_write(&gpio_nrf24_ce, true);
    furi_delay_us(150); /* Tstby2a */
}

void nrf24_tx_mode(void) {
    furi_hal_gpio_write(&gpio_nrf24_ce, false);
    uint8_t config = nrf24_read_reg(NRF24_REG_CONFIG);
    nrf24_write_reg(NRF24_REG_CONFIG, (config & ~CONFIG_PRIM_RX) | CONFIG_PWR_UP);
    furi_delay_us(150);
}

void nrf24_standby(void) {
    furi_hal_gpio_write(&gpio_nrf24_ce, false);
    uint8_t config = nrf24_read_reg(NRF24_REG_CONFIG);
    nrf24_write_reg(NRF24_REG_CONFIG, config | CONFIG_PWR_UP);
}

bool nrf24_tx(const uint8_t* payload, size_t size) {
    if(size == 0 || size > 32) return false;

    uint8_t tx[1 + 32];
    uint8_t rx[1 + 32];
    tx[0] = NRF24_CMD_W_TX_PAYLOAD;
    memcpy(&tx[1], payload, size);
    if(!nrf24_xfer(tx, rx, size + 1)) return false;

    nrf24_clear_irq(NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);
    nrf24_power_up(true); /* Tpd2stby before CE may be asserted */
    nrf24_tx_mode();
    furi_hal_gpio_write(&gpio_nrf24_ce, true);
    furi_delay_us(15); /* > 10 us pulse */
    furi_hal_gpio_write(&gpio_nrf24_ce, false);

    /* Poll for completion; 10 ms is generous at 1 Mbps with retries. */
    for(uint32_t i = 0; i < 100; i++) {
        uint8_t status = nrf24_status();
        if(status & NRF24_STATUS_TX_DS) {
            nrf24_clear_irq(NRF24_STATUS_TX_DS);
            return true;
        }
        if(status & NRF24_STATUS_MAX_RT) {
            nrf24_clear_irq(NRF24_STATUS_MAX_RT);
            nrf24_flush_tx();
            return false;
        }
        furi_delay_us(100);
    }

    nrf24_flush_tx();
    return false;
}

bool nrf24_rx_available(void) {
    uint8_t fifo = nrf24_read_reg(NRF24_REG_FIFO_STATUS);
    return (fifo & FIFO_STATUS_RX_EMPTY) == 0;
}

bool nrf24_rx_read(uint8_t* payload, size_t size) {
    if(size == 0 || size > 32) return false;

    uint8_t tx[1 + 32];
    uint8_t rx[1 + 32];
    tx[0] = NRF24_CMD_R_RX_PAYLOAD;
    memset(&tx[1], NRF24_CMD_NOP, size);
    if(!nrf24_xfer(tx, rx, size + 1)) return false;

    memcpy(payload, &rx[1], size);
    nrf24_clear_irq(NRF24_STATUS_RX_DR);
    return true;
}

bool nrf24_test_carrier(uint8_t channel) {
    /* Datasheet "constant carrier": power up in PTX, disable auto-retr/acks,
     * write a payload once, then hold CE high. */
    nrf24_write_reg(NRF24_REG_EN_AA, 0x00);
    nrf24_write_reg(NRF24_REG_SETUP_RETR, 0x00);
    nrf24_set_channel(channel);
    nrf24_set_rate(Nrf24Rate1M);
    nrf24_power_up(true);
    nrf24_tx_mode();

    uint8_t payload[32] = {0xAA};
    uint8_t tx[33];
    uint8_t rx[33];
    tx[0] = NRF24_CMD_W_TX_PAYLOAD;
    memcpy(&tx[1], payload, sizeof(payload));
    if(!nrf24_xfer(tx, rx, sizeof(tx))) return false;

    furi_hal_gpio_write(&gpio_nrf24_ce, true);
    return true;
}

uint8_t nrf24_read_rpd(void) {
    return nrf24_read_reg(NRF24_REG_RPD) & 0x01;
}
