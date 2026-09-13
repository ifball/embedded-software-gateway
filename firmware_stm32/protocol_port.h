#pragma once

/*
 * This file is a migration sketch for STM32 + FreeRTOS.
 * Keep the frame format identical to common/protocol.hpp:
 *
 *   AA 55 LEN SEQ CMD PAYLOAD CHECKSUM
 *
 * LEN = CMD byte + payload bytes.
 * CHECKSUM = XOR of LEN, SEQ, CMD and PAYLOAD.
 */

#include <stdint.h>
#include <stddef.h>

#define PROTO_SOF1 0xAA
#define PROTO_SOF2 0x55
#define PROTO_MAX_PAYLOAD 64

typedef enum {
    CMD_GET_STATUS = 0x01,
    CMD_GET_SENSOR = 0x02,
    CMD_SET_LED = 0x03,
    CMD_SET_PWM = 0x04,
    CMD_RESET_ERROR = 0x05,
    CMD_ACK = 0x80,
    CMD_ERROR = 0x81
} proto_cmd_t;

typedef struct {
    uint8_t seq;
    proto_cmd_t cmd;
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint8_t payload_len;
} proto_frame_t;

uint8_t proto_checksum(const uint8_t *bytes, size_t begin, size_t end);
size_t proto_encode(const proto_frame_t *frame, uint8_t *out, size_t out_size);
