#include "status_led.h"
#include "main.h"

#define RUNL1_PORT   GPIOA
#define RUNL1_PIN    GPIO_PIN_2   /* PA2 - 프레임 수신 표시 */
#define RUNL2_PORT   GPIOA
#define RUNL2_PIN    GPIO_PIN_3   /* PA3 - 1초 하트비트 */

#define HEARTBEAT_PERIOD_MS   1000u

void StatusLed_ToggleFrameIndicator(void)
{
    HAL_GPIO_TogglePin(RUNL1_PORT, RUNL1_PIN);
}

/*
 * HAL_SYSTICK_Callback: HAL_Init()이 기본으로 설정하는 1ms SysTick 틱마다
 * HAL이 자동으로 호출하는 weak 콜백을 여기서 재정의한다(별도 등록 불필요).
 * CubeMX에서 SysTick 이외의 다른 틱소스(Timer 등)로 바꾼 경우 주기가 달라지므로,
 * Clock Configuration의 "Timebase Source"가 SysTick인지 확인할 것.
 */
void HAL_SYSTICK_Callback(void)
{
    static uint32_t msCounter = 0;

    msCounter++;
    if (msCounter >= HEARTBEAT_PERIOD_MS) {
        msCounter = 0;
        HAL_GPIO_TogglePin(RUNL2_PORT, RUNL2_PIN);
    }
}
