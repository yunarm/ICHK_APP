#include "status_led.h"
#include "main.h"
#include <stdbool.h>

#define RUNL1_PORT   GPIOA
#define RUNL1_PIN    GPIO_PIN_2   /* PA2 - 통신 활동 표시 */
#define RUNL2_PORT   GPIOA
#define RUNL2_PIN    GPIO_PIN_3   /* PA3 - 1초 하트비트 */

#define HEARTBEAT_PERIOD_MS        1000u

#define ACTIVITY_BLINK_PERIOD_MS    100u  /* 점멸 토글 간격 (5Hz 점멸) */
#define ACTIVITY_IDLE_TIMEOUT_MS    300u  /* 이 시간 동안 프레임 없으면 소등 */

static volatile bool s_frameFlag = false;
static volatile uint32_t s_lastActivityTick = 0;

void StatusLed_NotifyFrameReceived(void)
{
    s_frameFlag = true;
    s_lastActivityTick = HAL_GetTick();
}

/*
 * 통신 중에는 실제 프레임 속도와 무관하게 고정 속도(약 5Hz)로 점멸하고,
 * ACTIVITY_IDLE_TIMEOUT_MS 동안 새 프레임이 없으면 완전히 소등한다.
 * 프레임 레이트가 사람 눈의 깜빡임 인지 한계(약 20~30Hz)보다 빠를 때
 * "항상 켜진 것처럼" 보이는 문제를 방지한다.
 */
void StatusLed_ActivityPoll(void)
{
    static uint32_t lastBlinkTick = 0;
    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - s_lastActivityTick) > ACTIVITY_IDLE_TIMEOUT_MS) {
        HAL_GPIO_WritePin(RUNL1_PORT, RUNL1_PIN, GPIO_PIN_RESET); /* 무통신 - 소등 */
        return;
    }

    if ((uint32_t)(now - lastBlinkTick) >= ACTIVITY_BLINK_PERIOD_MS) {
        lastBlinkTick = now;
        HAL_GPIO_TogglePin(RUNL1_PORT, RUNL1_PIN);
    }
}

/*
 * HAL_GetTick() 기반 폴링. __WFI()는 SysTick(1ms마다)을 포함한 모든 인터럽트에서
 * 깨어나므로, main 루프가 __WFI() 직후 매번 이 함수를 호출하기만 하면 충분한
 * 해상도(수 ms 오차 이내)로 1초 주기를 만들 수 있다.
 */
void StatusLed_HeartbeatPoll(void)
{
    static uint32_t lastToggleTick = 0;
    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - lastToggleTick) >= HEARTBEAT_PERIOD_MS) {
        lastToggleTick = now;
        HAL_GPIO_TogglePin(RUNL2_PORT, RUNL2_PIN);
    }
}
