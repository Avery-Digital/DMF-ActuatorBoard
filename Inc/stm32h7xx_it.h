/*******************************************************************************
 * @file    Inc/stm32h7xx_it.h
 * @author  Cam
 * @brief   Interrupt Service Routine declarations
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef STM32H7XX_IT_H
#define STM32H7XX_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void SysTick_Handler(void);
void HardFault_Handler(void);
void DMA_STR0_IRQHandler(void);
void DMA_STR1_IRQHandler(void);
void USART2_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32H7XX_IT_H */
