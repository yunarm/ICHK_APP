/**
 * led_driver.h
 * TIM3 PWM+DMA 방식 WS2812B 구동, 74HCT08 SEL 라인 순차 제어
 * 개발사양서 4장 참조.
 *
 * ARR/CCR 값은 SYSCLK 64MHz, WS2812B 대표 타이밍(T0H 0.35us / T1H 0.7us) 가정.
 * 실사용 칩 데이터시트로 재확인 후 필요시 WS_CCR_0 / WS_CCR_1 값을 조정할 것.
 */
#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>
#include "frame_protocol.h"

#define WS_TIMER_ARR        79u   /* 64MHz * 1.25us - 1 */
#define WS_CCR_0             22u   /* '0'-code duty, T0H ~= 0.35us */
#define WS_CCR_1             45u   /* '1'-code duty, T1H ~= 0.7us  */
#define WS_CCR_DUMMY           0u   /* 래치용 dummy, duty 0 고정 */

#define CHIPS_PER_BAR          30u
#define BITS_PER_CHIP          24u
#define CCR_BUF_LEN  (CHIPS_PER_BAR * BITS_PER_CHIP + 3u) /* 720 data + 3 low pipeline slots */

void LED_DriverInit(void);
void LED_DriverPoll(void); /* Call on every main-loop iteration. */
extern volatile uint32_t g_ledDmaStartErrors;

/* HAL_TIM_PWM_PulseFinishedCallback 에서 호출됨 (led_driver.c 내부에 구현) */
void LED_OnPortTransferComplete(uint8_t portGroup /* 0=A, 1=B */);

#endif /* LED_DRIVER_H */
