/**
 * host_protocol.h
 * UART(DMA) + SPI2 슬레이브(DMA) 동시 프레임 수신, BAR별 더블버퍼 관리.
 * 개발사양서 3장 참조.
 */
#ifndef HOST_PROTOCOL_H
#define HOST_PROTOCOL_H

#include <stdint.h>
#include "frame_protocol.h"

void HostProtocol_Init(void);

/* led_driver.c가 BAR 전송을 시작하기 직전 호출: 해당 BAR의 최신(active) 색상데이터 포인터 반환 */
const uint8_t *HostProtocol_GetBarData(uint8_t barNum);

#endif /* HOST_PROTOCOL_H */
