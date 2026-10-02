#include "frame_protocol.h"
#include <string.h>

uint8_t Frame_ComputeXor(const uint8_t *buf, uint16_t len)
{
    uint8_t x = 0;
    for (uint16_t i = 0; i < len; i++) {
        x ^= buf[i];
    }
    return x;
}

/*
 * raw[0]      = STX
 * raw[1]      = BAR#
 * raw[2..91]  = 90byte 색상 데이터
 * raw[92]     = checksum (XOR of raw[1..91], 즉 BAR#+데이터)
 * raw[93]     = ETX
 */
bool Frame_Validate(const uint8_t *raw, LedFrame_t *out)
{
    if (raw[0] != FRAME_STX) {
        return false;
    }
    if (raw[FRAME_TOTAL_LEN - 1] != FRAME_ETX) {
        return false;
    }

    uint8_t calc = Frame_ComputeXor(&raw[1], 1 + FRAME_DATA_LEN); /* BAR# + 90byte */
    if (calc != raw[FRAME_TOTAL_LEN - 2]) {
        return false;
    }

    out->bar_num = raw[1];
    memcpy(out->data, &raw[2], FRAME_DATA_LEN);
    return true;
}
