#include "host_protocol.h"
#include "dfu_trigger.h"
#include "status_led.h"
#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef hspi2;

/* Hardware ring: half/full events protect sustained traffic; IDLE delivers
 * short bursts (including a single 94-byte frame) without waiting to fill it. */
#define UART_DMA_SIZE 1024u
#define UART_QUEUE_SIZE 2048u
#define UART_QUEUE_MASK (UART_QUEUE_SIZE - 1u)
static uint8_t uartDma[UART_DMA_SIZE];
static uint16_t uartDmaPos;
static uint8_t uartQueue[UART_QUEUE_SIZE];
static volatile uint16_t uartHead, uartTail;
static volatile bool uartResetParser, uartRestart;
static uint8_t uartFrame[FRAME_TOTAL_LEN];
static uint16_t uartUsed;

/* NSS defines one transaction. Receive is pre-armed while NSS is HIGH.
 * 256-byte circular storage absorbs oversized transfers until the NSS edge;
 * any half/full event makes that transaction invalid (more than 94 bytes). */
#define SPI_DMA_SIZE 256u
#define SPI_QUEUE_SLOTS 16u
#define SPI_NSS_PIN GPIO_PIN_12
static uint8_t spiDma[SPI_DMA_SIZE];
static uint8_t spiQueue[SPI_QUEUE_SLOTS][FRAME_TOTAL_LEN];
static volatile uint8_t spiHead, spiTail;
static volatile bool spiArmed, spiActive, spiTooLong, spiRestart, spiNeedStop;
volatile uint8_t g_spiReady;
static void SpiPoll(void);
static void SpiArm(void);
static void SpiNssInit(void);
static LedFrame_t barBuffer[BAR_COUNT][2];
static volatile uint8_t barActiveIdx[BAR_COUNT];
volatile HostProtocolStats_t g_hostStats;

static void PublishFrame(const LedFrame_t *frame)
{
    if (frame->bar_num == BAR_DFU_ENTRY_CODE) {
        DFU_RequestEntry();
        return;
    }
    if (frame->bar_num >= BAR_COUNT) return;
    /* SPI publishes in an ISR; UART publishes in main. Protect the entire
     * short copy and swap, and restore the previous interrupt mask. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint8_t next = (uint8_t)(1u - barActiveIdx[frame->bar_num]);
    memcpy(&barBuffer[frame->bar_num][next], frame, sizeof(*frame));
    __DMB();
    barActiveIdx[frame->bar_num] = next;
    __set_PRIMASK(mask);
}

void HostProtocol_Init(void)
{
    memset(barBuffer, 0, sizeof(barBuffer));
    memset((void *)barActiveIdx, 0, sizeof(barActiveIdx));
    memset((void *)&g_hostStats, 0, sizeof(g_hostStats));
    uartHead = uartTail = uartDmaPos = uartUsed = 0;
    uartResetParser = uartRestart = false;
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uartDma, UART_DMA_SIZE) != HAL_OK) {
        g_hostStats.uart_restart_failures++;
        uartRestart = true;
    }
    spiHead = spiTail = 0;
    spiArmed = spiActive = spiTooLong = spiNeedStop = false;
    spiRestart = true;
    g_spiReady = 0;
    SpiNssInit();
    SpiArm();
}

const uint8_t *HostProtocol_GetBarData(uint8_t barNum)
{
    if (barNum >= BAR_COUNT) return NULL;
    return barBuffer[barNum][barActiveIdx[barNum]].data;
}

bool HostProtocol_CopyBarData(uint8_t barNum, uint8_t *data90)
{
    if (barNum >= BAR_COUNT || data90 == NULL) return false;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    memcpy(data90, barBuffer[barNum][barActiveIdx[barNum]].data, FRAME_DATA_LEN);
    __set_PRIMASK(mask);
    return true;
}

static void QueueByte(uint8_t value)
{
    if (uartResetParser) return;
    uint16_t next = (uint16_t)((uartHead + 1u) & UART_QUEUE_MASK);
    if (next == uartTail) {
        g_hostStats.uart_queue_overflows++;
        uartResetParser = true;
        return;
    }
    uartQueue[uartHead] = value;
    __DMB();
    uartHead = next;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart->Instance != USART1) return;
    (void)size;
    /* Read the live DMA position: an older HT/TC event can be dispatched
     * after an IDLE interrupt. Its Size must not replay already queued data.
     * Keep HT enabled. Every half-buffer is serviced long before DMA wraps. */
    uint16_t pos = (uint16_t)(UART_DMA_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx));
    if (pos >= UART_DMA_SIZE) pos = 0;
    while (uartDmaPos != pos) {
        QueueByte(uartDma[uartDmaPos]);
        uartDmaPos++;
        if (uartDmaPos == UART_DMA_SIZE) uartDmaPos = 0;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;
    g_hostStats.uart_errors++;
    uartResetParser = true;
    uartRestart = true;
    /* HAL calls here after DMA abort completes. Restart from main so ISR
     * stays short; the stream parser resynchronizes after a lost byte. */
}

static void ParseUartByte(uint8_t value)
{
    if (uartUsed == 0 && value != FRAME_STX) {
        g_hostStats.uart_discarded_bytes++;
        return;
    }
    uartFrame[uartUsed++] = value;
    if (uartUsed != FRAME_TOTAL_LEN) return;
    StatusLed_NotifyFrameReceived();
    LedFrame_t frame;
    if (Frame_Validate(uartFrame, &frame) &&
        (frame.bar_num < BAR_COUNT || frame.bar_num == BAR_DFU_ENTRY_CODE)) {
        g_hostStats.uart_valid_frames++;
        uartUsed = 0;
        PublishFrame(&frame);
        return;
    }
    g_hostStats.uart_invalid_frames++;
    /* STX/ETX in valid color bytes are ordinary payload. Only after a full
     * candidate fails validation do we search for the next possible STX. */
    uint16_t skip = 1;
    while (skip < FRAME_TOTAL_LEN && uartFrame[skip] != FRAME_STX) skip++;
    uartUsed = (uint16_t)(FRAME_TOTAL_LEN - skip);
    memmove(uartFrame, uartFrame + skip, uartUsed);
    g_hostStats.uart_discarded_bytes += skip;
}

void HostProtocol_Poll(void)
{
    SpiPoll();
    if (uartResetParser) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        uartTail = uartHead;
        uartResetParser = false;
        uartUsed = 0;
        __set_PRIMASK(mask);
    }
    if (uartRestart && huart1.RxState == HAL_UART_STATE_READY) {
        uartDmaPos = 0;
        if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uartDma, UART_DMA_SIZE) == HAL_OK) {
            uartRestart = false;
        } else {
            g_hostStats.uart_restart_failures++;
        }
    }
    /* Bound main-loop work so LED servicing continues under continuous RX. */
    for (uint16_t n = 0; n < UART_QUEUE_SIZE; n++) {
        if (uartResetParser || uartTail == uartHead) break;
        uint8_t value = uartQueue[uartTail];
        __DMB();
        uartTail = (uint16_t)((uartTail + 1u) & UART_QUEUE_MASK);
        ParseUartByte(value);
    }
}

/* Configure EXTI on PB12 without changing its SPI2_NSS alternate function.
 * HAL_GPIO_Init(GPIO_MODE_IT_...) would disconnect hardware NSS. */
static void SpiNssInit(void)
{
    EXTI->EXTICR[3] = (EXTI->EXTICR[3] & ~EXTI_EXTICR4_EXTI12) |
                     EXTI_EXTICR4_EXTI12_0; /* Port B = 1 */
    EXTI->RTSR1 |= SPI_NSS_PIN;
    EXTI->FTSR1 |= SPI_NSS_PIN;
    EXTI->RPR1 = SPI_NSS_PIN;
    EXTI->FPR1 = SPI_NSS_PIN;
    EXTI->IMR1 |= SPI_NSS_PIN;
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

static void SpiFreeze(void)
{
    /* No blocking HAL abort/wait inside the NSS interrupt. */
    CLEAR_BIT(hspi2.Instance->CR2, SPI_CR2_RXDMAEN | SPI_CR2_ERRIE);
    __HAL_DMA_DISABLE(hspi2.hdmarx);
    __DSB();
    spiArmed = false;
    g_spiReady = 0;
    spiNeedStop = true;
    spiRestart = true;
}

static void SpiFinishTransaction(void)
{
    bool active = spiActive;
    spiActive = false;
    SpiFreeze();
    uint16_t count = (uint16_t)(SPI_DMA_SIZE - __HAL_DMA_GET_COUNTER(hspi2.hdmarx));
    uint32_t errors = hspi2.Instance->SR & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_FRE);
    /* At the last SCK edge the final byte may still be in the RX FIFO rather
     * than DMA RAM. DMA is now frozen; drain up to the hardware FIFO capacity. */
    for (uint8_t i = 0; i < 4u &&
         (hspi2.Instance->SR & SPI_SR_FRLVL) != 0u; i++) {
        uint8_t value = *(__IO uint8_t *)&hspi2.Instance->DR;
        if (count < SPI_DMA_SIZE) spiDma[count] = value;
        count++;
    }
    __HAL_SPI_DISABLE(&hspi2);
    if (!active) return; /* Startup / an unarmed transaction: nothing to publish. */
    StatusLed_NotifyFrameReceived();
    bool excess = spiTooLong ||
        __HAL_DMA_GET_FLAG(hspi2.hdmarx, __HAL_DMA_GET_HT_FLAG_INDEX(hspi2.hdmarx)) ||
        __HAL_DMA_GET_FLAG(hspi2.hdmarx, __HAL_DMA_GET_TC_FLAG_INDEX(hspi2.hdmarx));
    if (excess || count != FRAME_TOTAL_LEN || errors != 0u) {
        g_hostStats.spi_invalid_frames++;
        if (excess || count != FRAME_TOTAL_LEN) g_hostStats.spi_length_errors++;
        if (errors != 0u) g_hostStats.spi_errors++;
        return;
    }
    uint8_t next = (uint8_t)((spiHead + 1u) % SPI_QUEUE_SLOTS);
    if (next == spiTail) {
        g_hostStats.spi_queue_overflows++;
        return;
    }
    memcpy(spiQueue[spiHead], spiDma, FRAME_TOTAL_LEN);
    __DMB();
    spiHead = next;
}

void HostProtocol_SpiNssIRQ(void)
{
    bool rising = (EXTI->RPR1 & SPI_NSS_PIN) != 0u;
    bool falling = (EXTI->FPR1 & SPI_NSS_PIN) != 0u;
    EXTI->RPR1 = SPI_NSS_PIN;
    EXTI->FPR1 = SPI_NSS_PIN;
    bool high = HAL_GPIO_ReadPin(GPIOB, SPI_NSS_PIN) == GPIO_PIN_SET;
    /* If two edges accumulated before ISR service, their ordering is lost.
     * Never publish a possible mixture of adjacent transactions. */
    if ((rising && falling) || (rising && !high) || (falling && high)) {
        g_hostStats.spi_nss_missed_edges++;
        spiActive = false;
        SpiFreeze();
        __HAL_SPI_DISABLE(&hspi2);
        return;
    }
    if (falling) {
        if (spiArmed) {
            spiActive = true;
            spiTooLong = false;
        } else {
            g_hostStats.spi_unarmed_transactions++;
        }
    }
    if (rising) SpiFinishTransaction();
}

static void SpiArm(void)
{
    if (!spiRestart || HAL_GPIO_ReadPin(GPIOB, SPI_NSS_PIN) != GPIO_PIN_SET) return;
    /* No transfer may start during cleanup. Master must provide the documented
     * NSS HIGH gap. On an unexpected LOW transition, defer until its end. */
    if (spiNeedStop) {
        HAL_StatusTypeDef stop = HAL_SPI_DMAStop(&hspi2);
        spiNeedStop = false;
        if (stop != HAL_OK) g_hostStats.spi_restart_failures++;
    }
    __HAL_SPI_DISABLE(&hspi2);
    /* Flush residual FIFO and clear OVR before a new DMA transaction. */
    for (uint8_t i = 0; i < 4u &&
         (hspi2.Instance->SR & SPI_SR_FRLVL) != 0u; i++) {
        (void)*(__IO uint8_t *)&hspi2.Instance->DR;
    }
    __HAL_SPI_CLEAR_OVRFLAG(&hspi2);
    if (HAL_GPIO_ReadPin(GPIOB, SPI_NSS_PIN) != GPIO_PIN_SET) return;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (HAL_GPIO_ReadPin(GPIOB, SPI_NSS_PIN) == GPIO_PIN_SET) {
        spiTooLong = false;
        if (HAL_SPI_Receive_DMA(&hspi2, spiDma, SPI_DMA_SIZE) == HAL_OK) {
            spiArmed = true;
            g_spiReady = 1;
            spiRestart = false;
        } else {
            g_hostStats.spi_restart_failures++;
            spiNeedStop = true;
        }
    }
    __set_PRIMASK(mask);
}

static void SpiPoll(void)
{
    SpiArm();
    /* Validate/publish queued frames only in main; ISR just freezes and copies. */
    for (uint8_t n = 0; n < SPI_QUEUE_SLOTS && spiTail != spiHead; n++) {
        LedFrame_t frame;
        bool valid = Frame_Validate(spiQueue[spiTail], &frame) &&
                     (frame.bar_num < BAR_COUNT || frame.bar_num == BAR_DFU_ENTRY_CODE);
        __DMB();
        spiTail = (uint8_t)((spiTail + 1u) % SPI_QUEUE_SLOTS);
        if (valid) {
            g_hostStats.spi_valid_frames++;
            PublishFrame(&frame);
        } else {
            g_hostStats.spi_invalid_frames++;
        }
    }
}

void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2 && spiActive) spiTooLong = true;
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2 && spiActive) spiTooLong = true;
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI2) return;
    g_hostStats.spi_errors++;
    spiActive = false;
    SpiFreeze();
    __HAL_SPI_DISABLE(&hspi2);
}
