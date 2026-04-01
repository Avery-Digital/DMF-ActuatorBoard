/*******************************************************************************
 * @file    Src/stm32h7xx_it.c
 * @author  Cam
 * @brief   Interrupt Service Routines
 *
 *          All ISR handlers live in this file.  Each handler does the
 *          minimum work required (clear flags, call driver callback)
 *          and returns.  Heavy processing is deferred to the main loop.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#include "Bsp.h"
#include "Usart_Driver.h"
#include "ll_tick.h"

/* ==========================================================================
 *  SYSTEM TICK (1 ms)
 * ========================================================================== */
void SysTick_Handler(void)
{
    LL_IncTick();
}

/* ==========================================================================
 *  HARD FAULT HANDLER
 * ========================================================================== */
void HardFault_Handler(void)
{
    volatile uint32_t active_irq = SCB->ICSR & 0x1FFU;
    volatile uint32_t hfsr = SCB->HFSR;
    volatile uint32_t cfsr = SCB->CFSR;
    (void)active_irq;
    (void)hfsr;
    (void)cfsr;
    while (1);
}

/* ==========================================================================
 *  DMA1 STREAM 0 — USART2 TX COMPLETE
 *
 *  Fires when DMA has finished loading all bytes into USART TDR.
 *  The TxCompleteISR waits for USART TC flag, then switches RS485
 *  back to receive mode.
 * ========================================================================== */
void DMA_STR0_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_TC0(DMA1)) {
        LL_DMA_ClearFlag_TC0(DMA1);
        USART_Driver_TxCompleteISR(&usart2_handle);
    }
    if (LL_DMA_IsActiveFlag_TE0(DMA1)) {
        LL_DMA_ClearFlag_TE0(DMA1);
        USART_Driver_TxCompleteISR(&usart2_handle);
    }
}

/* ==========================================================================
 *  DMA1 STREAM 1 — USART2 RX (HALF-TRANSFER and TRANSFER-COMPLETE)
 *
 *  HT fires when the circular buffer is half full.
 *  TC fires when the buffer wraps back to the beginning.
 *  Both trigger processing of any new bytes in the ring.
 * ========================================================================== */
void DMA_STR1_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_HT1(DMA1)) {
        LL_DMA_ClearFlag_HT1(DMA1);
        USART_Driver_RxProcessISR(&usart2_handle);
    }
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        USART_Driver_RxProcessISR(&usart2_handle);
    }
    if (LL_DMA_IsActiveFlag_TE1(DMA1)) {
        LL_DMA_ClearFlag_TE1(DMA1);
    }
}

/* ==========================================================================
 *  USART2 — IDLE LINE INTERRUPT
 *
 *  Fires when the UART RX line goes idle after receiving data.  This is
 *  the key interrupt for low-latency reception — it catches the end of
 *  a packet even if the DMA buffer is nowhere near half full.
 * ========================================================================== */
void USART2_IRQHandler(void)
{
    if (LL_USART_IsActiveFlag_IDLE(USART2)) {
        LL_USART_ClearFlag_IDLE(USART2);
        USART_Driver_RxProcessISR(&usart2_handle);
    }

    if (LL_USART_IsActiveFlag_ORE(USART2)) {
        LL_USART_ClearFlag_ORE(USART2);
    }

    if (LL_USART_IsActiveFlag_FE(USART2)) {
        LL_USART_ClearFlag_FE(USART2);
    }

    if (LL_USART_IsActiveFlag_NE(USART2)) {
        LL_USART_ClearFlag_NE(USART2);
    }
}
