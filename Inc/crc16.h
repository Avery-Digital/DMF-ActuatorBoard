/*******************************************************************************
 * @file    Inc/crc16.h
 * @author  Cam
 * @brief   CRC-16 CCITT (0x1021) — Lookup Table Implementation
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef CRC16_H
#define CRC16_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CRC16_INIT  0xFFFFU

uint16_t CRC16_Calc(const uint8_t *buf, uint16_t len);
uint16_t CRC16_Update(uint16_t crc, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_H */
