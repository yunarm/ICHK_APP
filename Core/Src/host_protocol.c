#include "host_protocol.h"
#include "dfu_trigger.h"
#include "status_led.h"
#include "main.h"
#include <string.h>

/* CubeMX가 생성하는 핸들 (main.c / usart.c / spi.c 에 정의됨) */
extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef  hspi2;

static uint8_t uartRxRaw[FRAME_TOTAL_LEN];
static uint8_t spiRxRaw[FRAME_TOTAL_LEN];

/* BAR별 더블버퍼: [barNum][0/1], activeIdx가 가리키는 쪽이 "읽기용(최신 확정본)" */
static LedFrame_t barBuffer[BAR_COUNT][2];
static volatile uint8_t barActiveIdx[BAR_COUNT];

static void HandleIncomingFrame(const uint8_t *raw);

void HostProtocol_Init(void)
{
    memset(barBuffer, 0, sizeof(barBuffer));
    memset((void *)barActiveIdx, 0, sizeof(barActiveIdx));

    HAL_UART_Receive_DMA(&huart1, uartRxRaw, FRAME_TOTAL_LEN);
    HAL_SPI_Receive_DMA(&hspi2, spiRxRaw, FRAME_TOTAL_LEN);
}

const uint8_t *HostProtocol_GetBarData(uint8_t barNum)
{
    if (barNum >= BAR_COUNT) {
        return NULL;
    }
    return barBuffer[barNum][barActiveIdx[barNum]].data;
}

static void HandleIncomingFrame(const uint8_t *raw)
{
    StatusLed_ToggleFrameIndicator(); /* PA2(RUNL_1): 유효성과 무관하게 프레임 수신마다 반전 */

    LedFrame_t frame;
    if (!Frame_Validate(raw, &frame)) {
        return; /* 체크섬/ETX 불일치 -> 조용히 폐기 */
    }

    if (frame.bar_num == BAR_DFU_ENTRY_CODE) {
        DFU_RequestEntry();
        return; /* NVIC_SystemReset() 호출로 여기서 리턴하지 않음 */
    }

    if (frame.bar_num >= BAR_COUNT) {
        return;
    }

    uint8_t writeIdx = (uint8_t)(1u - barActiveIdx[frame.bar_num]);
    memcpy(&barBuffer[frame.bar_num][writeIdx], &frame, sizeof(LedFrame_t));

    /* 포인터 스왑만 원자적으로 - LED 시퀀서(TIM DMA 콜백, 인터럽트 컨텍스트)와 경합 방지 */
    __disable_irq();
    barActiveIdx[frame.bar_num] = writeIdx;
    __enable_irq();
}

/* ---- HAL 콜백 : CubeMX 생성 stm32g0xx_it.c 의 USART1/SPI2 DMA IRQ에서 자동 호출됨 ----
 * DMA가 Circular 모드이므로 정상 완료 시에는 재무장(HAL_xxx_Receive_DMA 재호출)이
 * 불필요하다 - 자동으로 버퍼 처음으로 돌아가 계속 수신한다. 단, 에러 발생 시에는
 * HAL이 수신을 중단시키므로 에러 콜백에서는 재무장이 반드시 필요하다. */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) {
        return;
    }
    HandleIncomingFrame(uartRxRaw);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) {
        return;
    }
    HAL_UART_Receive_DMA(&huart1, uartRxRaw, FRAME_TOTAL_LEN); /* Circular 중단 복구 */
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI2) {
        return;
    }
    HandleIncomingFrame(spiRxRaw);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI2) {
        return;
    }
    HAL_SPI_Receive_DMA(&hspi2, spiRxRaw, FRAME_TOTAL_LEN); /* Circular 중단 복구 */
}
