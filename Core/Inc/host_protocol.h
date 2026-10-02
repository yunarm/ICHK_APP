#ifndef HOST_PROTOCOL_H
#define HOST_PROTOCOL_H
#include <stdint.h>
#include "frame_protocol.h"

/* UART statistics: inspect g_hostStats in the debugger. */
typedef struct {
    uint32_t uart_valid_frames;
    uint32_t uart_invalid_frames;
    uint32_t uart_discarded_bytes;
    uint32_t uart_errors;
    uint32_t uart_queue_overflows;
    uint32_t uart_restart_failures;
    uint32_t spi_valid_frames;
    uint32_t spi_invalid_frames;
    uint32_t spi_errors;
    uint32_t spi_length_errors;
    uint32_t spi_queue_overflows;
    uint32_t spi_restart_failures;
    uint32_t spi_nss_missed_edges;
    uint32_t spi_unarmed_transactions;
} HostProtocolStats_t;
extern volatile HostProtocolStats_t g_hostStats;

extern volatile uint8_t g_spiReady; /* 1: pre-armed for next NSS LOW. */
void HostProtocol_SpiNssIRQ(void); /* EXTI4_15 IRQ, PB12 only. */
void HostProtocol_Init(void);
/* Call every main-loop iteration, before LED_DriverPoll(). */
void HostProtocol_Poll(void);
/* Copies a consistent 90-byte snapshot; caller owns the destination. */
bool HostProtocol_CopyBarData(uint8_t barNum, uint8_t *data90);
/* Compatibility only: use CopyBarData for work outside interrupts. */
const uint8_t *HostProtocol_GetBarData(uint8_t barNum);
#endif
