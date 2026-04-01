/*******************************************************************************
 * @file    Inc/main.h
 * @author  Cam
 * @brief   Main Header — DMF Actuator Board
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Bsp.h"
#include "Clock_Config.h"
#include "Usart_Driver.h"
#include "Packet_Protocol.h"
#include "Command.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================= Deferred TX Request ============================ */

typedef struct {
    volatile bool   pending;
    uint8_t         msg1, msg2, cmd1, cmd2;
    uint8_t         payload[PKT_MAX_PAYLOAD];
    uint16_t        length;
} TxRequest;

extern TxRequest    tx_request;

/* ========================= Exported Functions ============================= */

void Error_Handler(uint32_t fault_code);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
