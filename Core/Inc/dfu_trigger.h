/**
 * dfu_trigger.h
 * HOST가 BAR#=0xFF 프레임으로 DFU 진입을 요청했을 때, 매직값을 남기고
 * 소프트웨어 리셋하여 부트로더가 DFU 모드로 진입하도록 한다.
 *
 * 중요: DFU_MAGIC_ADDR는 APP과 DFU_Firmware 양쪽의 링커스크립트에서
 * 동일한 물리 주소(.noinit 섹션)를 가리켜야 한다. 개발사양서 5.1절 참조.
 */
#ifndef DFU_TRIGGER_H
#define DFU_TRIGGER_H

#include <stdint.h>

#define DFU_MAGIC_VALUE   0xDEADBEEFu

/* 링커스크립트에서 .noinit 섹션을 SRAM 최상단 4byte(0x20008FF0 등)에 고정 배치할 것 */
extern volatile uint32_t g_dfuMagic __attribute__((section(".noinit")));

void DFU_RequestEntry(void); /* 매직값 기록 + NVIC_SystemReset(), 리턴하지 않음 */

#endif /* DFU_TRIGGER_H */
