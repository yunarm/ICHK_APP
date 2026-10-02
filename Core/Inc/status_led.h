/**
 * status_led.h
 * 동작 상태 표시 LED 제어.
 * - RUNL_1 (PA2): 호스트 통신 활동 표시
 * - RUNL_2 (PA3): 1초마다 반전 (프로그램 동작 중 표시, 하트비트)
 *
 * [v1.2 개선] RUNL_1을 "프레임마다 토글"에서 "고정 속도(5Hz) 점멸 + 무통신시
 * 소등" 방식으로 변경. 프레임 레이트가 높으면 사람 눈에는 토글이 합쳐져 항상
 * 켜진 것처럼 보이는 문제(≈20~30Hz 이상은 깜빡임으로 인지 불가)를 해결한다.
 *
 * [v1.1 버그 수정] HAL_SYSTICK_Callback()에 의존하던 하트비트(RUNL_2) 구현을
 * CubeMX가 기본 생성하는 SysTick_Handler가 이 콜백을 부르지 않는 문제로 인해
 * HAL_GetTick() 기반 폴링 방식으로 변경함 (유지).
 */
#ifndef STATUS_LED_H
#define STATUS_LED_H

/* host_protocol.c 에서 프레임 하나(UART/SPI, 유효성 검증 전)를 받을 때마다 호출.
 * 인터럽트(DMA 콜백) 컨텍스트에서 호출해도 안전하다 (플래그만 세팅). */
void StatusLed_NotifyFrameReceived(void);

/* main()의 메인 루프에서 매 반복마다 호출 - RUNL_1 활동표시 로직을 처리한다 */
void StatusLed_ActivityPoll(void);

/* main()의 메인 루프에서 매 반복마다 호출 - RUNL_2 1초 하트비트를 처리한다 */
void StatusLed_HeartbeatPoll(void);

#endif /* STATUS_LED_H */
