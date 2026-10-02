#include "led_driver.h"
#include "host_protocol.h"
#include "main.h"

extern TIM_HandleTypeDef htim3; /* CubeMX 생성 */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} SelLine_t;

/* SEL0~3 = 포트A(BAR0~3), SEL4~7 = 포트B(BAR4~7). 핀맵은 개발사양서 6장 참조. */
static const SelLine_t selLines[BAR_COUNT] = {
    { GPIOD, GPIO_PIN_0 }, /* SEL0 -> BAR0 */
    { GPIOD, GPIO_PIN_1 }, /* SEL1 -> BAR1 */
    { GPIOD, GPIO_PIN_2 }, /* SEL2 -> BAR2 */
    { GPIOD, GPIO_PIN_3 }, /* SEL3 -> BAR3 */
    { GPIOB, GPIO_PIN_3 }, /* SEL4 -> BAR4 */
    { GPIOB, GPIO_PIN_4 }, /* SEL5 -> BAR5 */
    { GPIOB, GPIO_PIN_5 }, /* SEL6 -> BAR6 */
    { GPIOB, GPIO_PIN_6 }, /* SEL7 -> BAR7 */
};

/* uint32_t 버퍼 채택: HAL_TIM_PWM_Start_DMA API가 요구하는 표준 타입과 일치시켜
 * DMA MemDataAlignment=WORD / PeriphDataAlignment=WORD 로 단순/견고하게 구성.
 * (사양서 초안의 byte-packing 최적화 대신 표준적인 방식으로 대체 - SRAM 여유 충분) */
static uint32_t ccrBufPortA[CCR_BUF_LEN];
static uint32_t ccrBufPortB[CCR_BUF_LEN];

static volatile bool portReady[2] = { true, true };
volatile uint32_t g_ledDmaStartErrors;

static uint8_t currentBarPortA = 0; /* 0..3  (BAR 0~3) */
static uint8_t currentBarPortB = 4; /* 4..7  (BAR 4~7) */

static void EncodeByte(uint8_t byteVal, uint32_t *out8)
{
    for (int8_t b = 7; b >= 0; b--) {
        *out8++ = (byteVal & (1u << b)) ? WS_CCR_1 : WS_CCR_0;
    }
}

static void EncodeBar(const uint8_t *data90, uint32_t *ccrBufOut)
{
    uint16_t idx = 0;
    for (uint16_t i = 0; i < CHIPS_PER_BAR * 3u; i++) {
        EncodeByte(data90[i], &ccrBufOut[idx]);
        idx += 8u;
    }
    /* Several zero entries ensure the last data pulse AND preload pipeline
     * have finished before stopping a channel. Deselected BARs remain low
     * while the other three BARs are scanned, providing the latch interval. */
    while (idx < CCR_BUF_LEN) ccrBufOut[idx++] = WS_CCR_DUMMY;
}

static void StartPortTransfer(uint8_t portGroup)
{
    uint8_t barIdx = (portGroup == 0) ? currentBarPortA : currentBarPortB;
    uint32_t *ccrBuf = (portGroup == 0) ? ccrBufPortA : ccrBufPortB;

    uint8_t data90[FRAME_DATA_LEN];
    if (HostProtocol_CopyBarData(barIdx, data90)) {
        EncodeBar(data90, ccrBuf);
    } else {
        for (uint16_t i = 0; i < CCR_BUF_LEN; i++) {
            ccrBuf[i] = (i < CHIPS_PER_BAR * BITS_PER_CHIP) ? WS_CCR_0 : WS_CCR_DUMMY; /* 데이터 없으면 안전하게 전부 꺼진 상태로 */
        }
    }

    /* The finished transfer leaves the active CCR at zero. If both
     * channels are stopped, reset the shared counter/preload while safe. */
    if ((htim3.Instance->CR1 & TIM_CR1_CEN) == 0u) {
        __HAL_TIM_SET_COUNTER(&htim3, 0);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
        htim3.Instance->EGR = TIM_EGR_UG;
    }
    portReady[portGroup] = false;
    HAL_GPIO_WritePin(selLines[barIdx].port, selLines[barIdx].pin, GPIO_PIN_SET);
    HAL_StatusTypeDef result;

    if (portGroup == 0) {
        result = HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, ccrBuf, CCR_BUF_LEN);
    } else {
        result = HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_2, ccrBuf, CCR_BUF_LEN);
    }
    if (result != HAL_OK) {
        HAL_GPIO_WritePin(selLines[barIdx].port, selLines[barIdx].pin, GPIO_PIN_RESET);
        g_ledDmaStartErrors++;
        Error_Handler();
    }
}

void LED_DriverInit(void)
{
    for (uint8_t i = 0; i < BAR_COUNT; i++) {
        HAL_GPIO_WritePin(selLines[i].port, selLines[i].pin, GPIO_PIN_RESET);
    }
    currentBarPortA = 0;
    currentBarPortB = 4;
    portReady[0] = portReady[1] = true;
    g_ledDmaStartErrors = 0;
}

/* Encoding takes place only in main, never in a DMA interrupt. */
void LED_DriverPoll(void)
{
    if (portReady[0]) StartPortTransfer(0);
    if (portReady[1]) StartPortTransfer(1);
}

void LED_OnPortTransferComplete(uint8_t portGroup)
{
    if (portGroup == 0) {
        HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_1);
        HAL_GPIO_WritePin(selLines[currentBarPortA].port, selLines[currentBarPortA].pin, GPIO_PIN_RESET);
        currentBarPortA = (uint8_t)((currentBarPortA + 1u) % 4u);
        portReady[0] = true;
    } else {
        HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_2);
        HAL_GPIO_WritePin(selLines[currentBarPortB].port, selLines[currentBarPortB].pin, GPIO_PIN_RESET);
        currentBarPortB = (uint8_t)(4u + ((currentBarPortB - 4u + 1u) % 4u));
        portReady[1] = true;
    }
}

/* ---- HAL 콜백 : DMA1 Channel1/2 IRQ (TIM3 CH1/CH2)에서 자동 호출됨 ---- */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM3) {
        return;
    }
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        LED_OnPortTransferComplete(0);
    } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        LED_OnPortTransferComplete(1);
    }
}
