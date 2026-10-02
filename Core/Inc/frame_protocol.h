/**
 * frame_protocol.h
 * HOST -> APP 고정 94byte LED 프레임 파서
 * 구조: STX(1) + BAR#(1) + 색상데이터(90) + XOR체크섬(1) + ETX(1)
 *
 * 개발사양서 3장 참조. 체크섬 = 8bit XOR (BAR#+90byte 데이터 전체)로 확정.
 */
#ifndef FRAME_PROTOCOL_H
#define FRAME_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define FRAME_STX               0x02u
#define FRAME_ETX                0x03u
#define FRAME_DATA_LEN            90u   /* 30 chip x 3 byte */
#define FRAME_TOTAL_LEN           94u   /* STX+BAR#+DATA+CHKSUM+ETX */
#define BAR_COUNT                  8u
#define BAR_DFU_ENTRY_CODE       0xFFu  /* 예약 코드: HOST가 이 값을 BAR#에 실어 보내면 DFU 진입 요청 */

typedef struct {
    uint8_t bar_num;
    uint8_t data[FRAME_DATA_LEN];   /* WS2812B 입력 순서(GRB)로 정렬되어 있다고 가정 - HOST 스펙 재확인 필요 */
} LedFrame_t;

/* raw: FRAME_TOTAL_LEN(94) byte DMA 수신 버퍼. out: 유효할 경우 파싱 결과가 채워짐. */
bool Frame_Validate(const uint8_t *raw, LedFrame_t *out);
uint8_t Frame_ComputeXor(const uint8_t *buf, uint16_t len);

#endif /* FRAME_PROTOCOL_H */
