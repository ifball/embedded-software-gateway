#include "protocol_port.h"

uint8_t proto_checksum(const uint8_t *bytes, size_t begin, size_t end) {
    uint8_t value = 0;
    for (size_t i = begin; i < end; ++i) {
        value ^= bytes[i];
    }
    return value;
}

size_t proto_encode(const proto_frame_t *frame, uint8_t *out, size_t out_size) {
    const uint8_t len = (uint8_t)(1 + frame->payload_len);
    const size_t total = 2 + 1 + 1 + len + 1;
    if (out_size < total || frame->payload_len > PROTO_MAX_PAYLOAD) {
        return 0;
    }

    out[0] = PROTO_SOF1;
    out[1] = PROTO_SOF2;
    out[2] = len;
    out[3] = frame->seq;
    out[4] = (uint8_t)frame->cmd;
    for (uint8_t i = 0; i < frame->payload_len; ++i) {
        out[5 + i] = frame->payload[i];
    }
    out[total - 1] = proto_checksum(out, 2, total - 1);
    return total;
}
