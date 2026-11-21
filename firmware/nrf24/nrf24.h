/**
 * @file nrf24.h
 * @brief Minimal nRF24L01+ driver for the "f55" board (SPI2).
 *
 * Bus:  furi_hal_spi_bus_handle_nrf24
 *       PD1 = SCK, PB15 = MOSI, PC2 = MISO
 *       PB0 = CSN, PC11 = CE, PB1 = IRQ (EXTI1)
 *
 * Scope: register access, channel/data-rate/PA setup, one TX pipe and one RX
 * pipe, blocking TX with status polling, and the two test modes used during
 * bring-up (constant carrier, RPD). Interrupt-driven RX is left to the caller.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Commands */
#define NRF24_CMD_R_REGISTER   0x00
#define NRF24_CMD_W_REGISTER   0x20
#define NRF24_CMD_R_RX_PAYLOAD 0x61
#define NRF24_CMD_W_TX_PAYLOAD 0xA0
#define NRF24_CMD_FLUSH_TX     0xE1
#define NRF24_CMD_FLUSH_RX     0xE2
#define NRF24_CMD_NOP          0xFF

/* Registers */
#define NRF24_REG_CONFIG      0x00
#define NRF24_REG_EN_AA       0x01
#define NRF24_REG_EN_RXADDR   0x02
#define NRF24_REG_SETUP_AW    0x03
#define NRF24_REG_SETUP_RETR  0x04
#define NRF24_REG_RF_CH       0x05
#define NRF24_REG_RF_SETUP    0x06
#define NRF24_REG_STATUS      0x07
#define NRF24_REG_OBSERVE_TX  0x08
#define NRF24_REG_RPD         0x09
#define NRF24_REG_RX_ADDR_P0  0x0A
#define NRF24_REG_TX_ADDR     0x10
#define NRF24_REG_RX_PW_P0    0x11
#define NRF24_REG_FIFO_STATUS 0x17
#define NRF24_REG_DYNPD       0x1C
#define NRF24_REG_FEATURE     0x1D

/* STATUS bits */
#define NRF24_STATUS_RX_DR  (1u << 6)
#define NRF24_STATUS_TX_DS  (1u << 5)
#define NRF24_STATUS_MAX_RT (1u << 4)

typedef enum {
    Nrf24Rate250k = 0,
    Nrf24Rate1M = 1,
    Nrf24Rate2M = 2,
} Nrf24Rate;

typedef enum {
    Nrf24PaMinus18dBm = 0,
    Nrf24PaMinus12dBm = 1,
    Nrf24PaMinus6dBm = 2,
    Nrf24Pa0dBm = 3,
} Nrf24Pa;

/** @brief Read CONFIG and verify it can be written; false if no module. */
bool nrf24_init(void);

uint8_t nrf24_read_reg(uint8_t reg);
void nrf24_write_reg(uint8_t reg, uint8_t value);
void nrf24_read_buf(uint8_t reg, uint8_t* data, size_t size);
void nrf24_write_buf(uint8_t reg, const uint8_t* data, size_t size);

uint8_t nrf24_status(void);
void nrf24_clear_irq(uint8_t mask);
void nrf24_flush_tx(void);
void nrf24_flush_rx(void);

void nrf24_set_channel(uint8_t channel); /* 0..125, 2400 + channel MHz */
void nrf24_set_rate(Nrf24Rate rate);
void nrf24_set_pa(Nrf24Pa pa);
void nrf24_set_address_width(uint8_t width); /* 3..5 bytes */
void nrf24_set_rx_address(uint8_t pipe, const uint8_t* address, size_t size);
void nrf24_set_tx_address(const uint8_t* address, size_t size);
void nrf24_set_payload_width(uint8_t pipe, uint8_t width);

void nrf24_power_up(bool enabled);
void nrf24_rx_mode(void);
void nrf24_tx_mode(void);
void nrf24_standby(void);

/** @brief Blocking TX of one payload. Returns true on TX_DS. */
bool nrf24_tx(const uint8_t* payload, size_t size);

bool nrf24_rx_available(void);
bool nrf24_rx_read(uint8_t* payload, size_t size);

/** @brief Continuous carrier on a channel (bring-up / spectrum test). */
bool nrf24_test_carrier(uint8_t channel);

/** @brief Received power detector (one bit: 1 = signal above -64 dBm). */
uint8_t nrf24_read_rpd(void);

#ifdef __cplusplus
}
#endif
