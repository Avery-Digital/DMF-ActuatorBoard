/*******************************************************************************
 * @file    Inc/Clock_Config.h
 * @author  Cam
 * @brief   Clock Configuration — MCU Init and Clock Tree Setup
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Bsp.h"

void MCU_Init(void);
void ClockTree_Init(const ClockTree_Config *clk);
void Error_Handler(uint32_t fault_code);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_CONFIG_H */
