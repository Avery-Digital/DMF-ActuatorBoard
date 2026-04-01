/*******************************************************************************
 * @file    Inc/Usart_Driver.h
 * @author  Cam
 * @brief   USART Driver — Interrupt-Driven DMA TX/RX with RS485 Direction
 *
 *          Reception is fully interrupt-driven:
 *            - DMA HT/TC interrupts catch bulk data
 *            - USART IDLE interrupt catches end-of-packet gaps
 *          All three feed bytes into the protocol parser from ISR context.
 *
 *          Transmission uses DMA normal mode with RS485 DE/RE toggling.
 *          DE/RE are asserted before TX and de-asserted in the TX complete ISR.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef USART_DRIVER_H
#define USART_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Bsp.h"
#include "Packet_Protocol.h"

/* =========================== Public API =================================== */

InitResult USART_Driver_Init(USART_Handle *handle, ProtocolParser *parser);

void USART_Driver_StartRx(USART_Handle *handle);

InitResult USART_Driver_SendPacket(USART_Handle *handle,
                                   uint8_t msg1, uint8_t msg2,
                                   uint8_t cmd1, uint8_t cmd2,
                                   const uint8_t *payload, uint16_t length);

InitResult USART_Driver_Transmit(USART_Handle *handle,
                                 const uint8_t *data, uint16_t len);

/* ======================== ISR Callbacks ==================================== */

void USART_Driver_RxProcessISR(USART_Handle *handle);
void USART_Driver_TxCompleteISR(USART_Handle *handle);

/* ===================== RS485 Direction Control ============================= */

void RS485_SetTxMode(void);
void RS485_SetRxMode(void);
void RS485_Init_Pins(void);

#ifdef __cplusplus
}
#endif

#endif /* USART_DRIVER_H */
