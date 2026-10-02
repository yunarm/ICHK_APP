#include "dfu_trigger.h"
#include "main.h"

volatile uint32_t g_dfuMagic __attribute__((section(".noinit")));

void DFU_RequestEntry(void)
{
    g_dfuMagic = DFU_MAGIC_VALUE;
    __disable_irq();
    NVIC_SystemReset(); /* 리턴하지 않음 */
}
