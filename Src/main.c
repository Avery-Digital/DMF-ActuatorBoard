/*******************************************************************************
 * @file    Src/main.c
 * @author  Cam
 * @brief   Main Entry Point — DMF Actuator Board
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 *
 * Initialization sequence:
 *   1.  MCU_Init()            — MPU, SYSCFG, NVIC, flash latency, power
 *   2.  ClockTree_Init()      — HSI → PLL1 (480 MHz SYSCLK), PLL2 (128 MHz)
 *   2a. SysTick 1 ms          — LL_Init1msTick + LL_SYSTICK_EnableIT
 *   3.  Protocol_ParserInit() — Frame parser state machine
 *   4.  USART_Driver_Init()   — GPIO, USART2, DMA streams, RS485 pins
 *   5.  USART_Driver_StartRx()— Enable DMA HT/TC + USART IDLE interrupts
 *
 * Runtime:
 *   All UART reception is interrupt-driven.  The main loop polls
 *   tx_request.pending and sends deferred responses.
 *
 *   OnPacketReceived() is called from ISR context (DMA or USART IDLE).
 *   Command handlers MUST be fast — set tx_request.pending and return.
 ******************************************************************************/

#include "main.h"
#include "Actuator.h"
#include <string.h>
#include "stm32h7xx_ll_rtc.h"

/* Protocol parser instance */
static ProtocolParser usart2_parser;

/* Deferred TX request — set by ISR, consumed by main loop */
TxRequest tx_request = {0};

/* Private function prototypes -----------------------------------------------*/
static void SystemInit_Sequence(void);
static void OnPacketReceived(const PacketHeader *header,
                             const uint8_t *payload,
                             void *ctx);

/* ==========================================================================
 *  ENTRY POINT
 * ========================================================================== */
int main(void)
{
    SystemInit_Sequence();

    while (1)
    {
        /* Consume pending TX outside of ISR context */
        if (tx_request.pending) {
            tx_request.pending = false;
            USART_Driver_SendPacket(&usart2_handle,
                                    tx_request.msg1,
                                    tx_request.msg2,
                                    tx_request.cmd1,
                                    tx_request.cmd2,
                                    tx_request.payload,
                                    tx_request.length);
        }
    }
}

/* ==========================================================================
 *  SYSTEM INITIALIZATION
 * ========================================================================== */
static void SystemInit_Sequence(void)
{
    InitResult usart_result;

    /* Step 1: MCU core — MPU, flash latency, voltage scaling */
    MCU_Init();

    /* Step 2: Clock tree — HSI → PLL1 (480 MHz), PLL2 (128 MHz USART) */
    ClockTree_Init(&sys_clk_config);

    /* Step 2a: SysTick for 1 ms interrupts */
    LL_Init1msTick(sys_clk_config.sysclk_hz);
    LL_SYSTICK_EnableIT();

    /* Step 3: Actuator GPIO — 28 outputs via L293Q, all OFF, drivers disabled */
    Actuator_Init();

    /* Step 4: Protocol parser — register the packet callback */
    Protocol_ParserInit(&usart2_parser, OnPacketReceived, NULL);

    /* Step 5: USART2 + DMA on PA2 (TX) / PA3 (RX)
     *         RS485 DE/RE on PA5/PA4 (LTC2864)
     *         Parser is passed to the driver so ISRs can feed it directly */
    usart_result = USART_Driver_Init(&usart2_handle, &usart2_parser);
    if (usart_result != INIT_OK) {
        Error_Handler(0x11);
    }

    /* Step 6: Start interrupt-driven DMA reception */
    USART_Driver_StartRx(&usart2_handle);
}

/* ==========================================================================
 *  PACKET RECEIVED CALLBACK
 *
 *  Called by the protocol parser from ISR context (DMA HT/TC or USART IDLE).
 *  Routes to Command_Dispatch which populates tx_request.
 * ========================================================================== */
static void OnPacketReceived(const PacketHeader *header,
                             const uint8_t *payload,
                             void *ctx)
{
    (void)ctx;
    Command_Dispatch(&usart2_handle, header, payload);
}

/* ==========================================================================
 *  ERROR HANDLER
 * ========================================================================== */
void Error_Handler(uint32_t fault_code)
{
    __disable_irq();
    LL_PWR_EnableBkUpAccess();
    LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR0, fault_code);
    while (1);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    while (1);
}
#endif
