/**
 * status_led.h
 * 동작 상태 표시 LED 제어.
 * - RUNL_1 (PA2): 호스트로부터 프레임이 수신될 때마다 반전 (통신 활동 표시)
 * - RUNL_2 (PA3): 1초마다 반전 (프로그램 동작 중 표시, 하트비트)
 */
#ifndef STATUS_LED_H
#define STATUS_LED_H

/* host_protocol.c 에서 프레임 하나(UART/SPI, 유효성 검증 전)를 받을 때마다 호출 */
void StatusLed_ToggleFrameIndicator(void);

#endif /* STATUS_LED_H */
