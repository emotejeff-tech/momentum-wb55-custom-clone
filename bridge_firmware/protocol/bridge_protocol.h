/**
 * @file bridge_protocol.h
 * @brief Wire format shared by the STM32 firmware and companion bridges.
 *
 * Transport: USART1 921600 8N1, USB CDC or BLE serial.
 * Framing:   COBS on every frame, 0x00 as the only delimiter.
 * Integrity: CRC-16/CCITT-FALSE over the decoded frame (header + payload).
 *
 * Keep this file byte-compatible with bridge_firmware/pi_bridge/bridge.py
 * (the Python implementation mirrors these constants).
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BRIDGE_VERSION       0x01
#define BRIDGE_MAX_PAYLOAD   1024
#define BRIDGE_HEADER_SIZE   6
#define BRIDGE_MAX_FRAME     (BRIDGE_HEADER_SIZE + BRIDGE_MAX_PAYLOAD + 2)

/* Frame flags */
#define BRIDGE_FLAG_ACK_REQ  0x01
#define BRIDGE_FLAG_RESPONSE 0x02
#define BRIDGE_FLAG_MORE     0x04

/* Message types */
typedef enum {
    BridgeTypeHello = 0x01,
    BridgeTypeAck = 0x02,
    BridgeTypeHeartbeat = 0x03,
    BridgeTypeStatusReq = 0x10,
    BridgeTypeStatus = 0x11,
    BridgeTypeAiReq = 0x20,
    BridgeTypeAiResp = 0x21,
    BridgeTypeAiChunk = 0x22,
    BridgeTypeCmd = 0x30,
    BridgeTypeCmdResult = 0x31,
    BridgeTypeTelemetry = 0x40,
    BridgeTypeEvent = 0x41,
    BridgeTypeLog = 0x50,
    BridgeTypeError = 0x7F,
} BridgeType;

/* Decoded frame view into a caller-owned buffer. */
typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t seq;
    uint8_t flags;
    uint16_t payload_len;
    const uint8_t* payload; /* not NUL-terminated */
} BridgeFrame;

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0. */
static inline uint16_t bridge_crc16(const uint8_t* data, size_t size) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/** COBS-encode `len` bytes; returns encoded length, or 0 on overflow. */
static inline size_t bridge_cobs_encode(
    const uint8_t* src,
    size_t len,
    uint8_t* dst,
    size_t dst_cap) {
    if(dst_cap < 2) return 0;

    size_t read = 0;
    size_t write = 1;
    size_t code_pos = 0;
    uint8_t code = 1;

    while(read < len) {
        if(src[read] == 0x00) {
            dst[code_pos] = code;
            code = 1;
            code_pos = write++;
            read++;
            if(write > dst_cap) return 0;
        } else {
            if(write >= dst_cap) return 0;
            dst[write++] = src[read++];
            code++;
            if(code == 0xFF) {
                dst[code_pos] = code;
                code = 1;
                code_pos = write++;
                if(write > dst_cap) return 0;
            }
        }
    }
    dst[code_pos] = code;
    return write;
}

/** COBS-decode `len` bytes; returns decoded length, or 0 on malformed input. */
static inline size_t bridge_cobs_decode(
    const uint8_t* src,
    size_t len,
    uint8_t* dst,
    size_t dst_cap) {
    size_t read = 0;
    size_t write = 0;

    while(read < len) {
        uint8_t code = src[read++];
        if(code == 0) return 0;

        for(uint8_t i = 1; i < code; i++) {
            if(read >= len || write >= dst_cap) return 0;
            dst[write++] = src[read++];
        }
        /* A zero is implied after every block except the last. */
        if(code < 0xFF && read < len) {
            if(write >= dst_cap) return 0;
            dst[write++] = 0x00;
        }
    }
    return write;
}

/** Build a complete delimited frame. Returns encoded length (incl. 0x00), or 0. */
static inline size_t bridge_frame_build(
    uint8_t type,
    uint8_t seq,
    uint8_t flags,
    const uint8_t* payload,
    uint16_t payload_len,
    uint8_t* out,
    size_t out_cap) {
    if(payload_len > BRIDGE_MAX_PAYLOAD || out_cap < 2) return 0;

    uint8_t raw[BRIDGE_HEADER_SIZE + BRIDGE_MAX_PAYLOAD + 2];
    raw[0] = BRIDGE_VERSION;
    raw[1] = type;
    raw[2] = seq;
    raw[3] = flags;
    raw[4] = (uint8_t)(payload_len & 0xFF);
    raw[5] = (uint8_t)(payload_len >> 8);
    if(payload_len && payload) memcpy(&raw[BRIDGE_HEADER_SIZE], payload, payload_len);

    uint16_t crc = bridge_crc16(raw, (size_t)BRIDGE_HEADER_SIZE + payload_len);
    raw[BRIDGE_HEADER_SIZE + payload_len] = (uint8_t)(crc & 0xFF);
    raw[BRIDGE_HEADER_SIZE + payload_len + 1] = (uint8_t)(crc >> 8);

    size_t encoded = bridge_cobs_encode(
        raw, (size_t)BRIDGE_HEADER_SIZE + payload_len + 2, out, out_cap - 1);
    if(encoded == 0) return 0;
    out[encoded++] = 0x00;
    return encoded;
}

/**
 * Parse one COBS frame (without the trailing 0x00).
 * `decoded` must provide BRIDGE_MAX_FRAME bytes. On success `frame` points into
 * `decoded`.
 */
static inline bool bridge_frame_parse(
    const uint8_t* encoded,
    size_t encoded_len,
    uint8_t* decoded,
    size_t decoded_cap,
    BridgeFrame* frame) {
    size_t len = bridge_cobs_decode(encoded, encoded_len, decoded, decoded_cap);
    if(len < BRIDGE_HEADER_SIZE + 2) return false;

    uint16_t payload_len = (uint16_t)decoded[4] | ((uint16_t)decoded[5] << 8);
    if(payload_len > BRIDGE_MAX_PAYLOAD) return false;
    if(len != (size_t)BRIDGE_HEADER_SIZE + payload_len + 2) return false;

    uint16_t crc = (uint16_t)decoded[BRIDGE_HEADER_SIZE + payload_len] |
                   ((uint16_t)decoded[BRIDGE_HEADER_SIZE + payload_len + 1] << 8);
    if(bridge_crc16(decoded, (size_t)BRIDGE_HEADER_SIZE + payload_len) != crc) return false;

    frame->version = decoded[0];
    frame->type = decoded[1];
    frame->seq = decoded[2];
    frame->flags = decoded[3];
    frame->payload_len = payload_len;
    frame->payload = &decoded[BRIDGE_HEADER_SIZE];
    return frame->version == BRIDGE_VERSION;
}

#ifdef __cplusplus
}
#endif
