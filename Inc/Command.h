/*******************************************************************************
 * @file    Inc/Command.h
 * @author  Cam
 * @brief   Command Definitions and Dispatch for DMF Actuator Board
 *
 *          Command range: 0x0F00 – 0x10FF (reserved for actuator board)
 *
 *          Command codes are 16-bit values formed from CMD1 (high byte)
 *          and CMD2 (low byte):
 *            CMD_CODE(0x0F, 0x00) → 0x0F00
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef COMMAND_H
#define COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Bsp.h"
#include "Packet_Protocol.h"

/* ========================= Command Code Macro ============================= */

#define CMD_CODE(c1, c2)    ((uint16_t)((c1) << 8) | (c2))

/* ========================= Shared Commands ================================ */

#define CMD_PING            CMD_CODE(0xDE, 0xAD)    /**< Ping / echo test        */

/* ========================= Actuator Board Commands ======================== */

/*  Range: 0x0F00 – 0x10FF
 *  Actuator board commands for 28 GPIO-driven half-bridges (L293Q). */

#define CMD_GET_BOARD_TYPE  CMD_CODE(0x0B, 0x99)    /**< Board type (shared cmd) */

/* ---- Actuator Commands (0x0F00–0x0F1F) ---- */
#define CMD_ACT_SET         CMD_CODE(0x0F, 0x00)    /**< Set single actuator     */
#define CMD_ACT_GET         CMD_CODE(0x0F, 0x01)    /**< Get single actuator     */
#define CMD_ACT_SET_ALL     CMD_CODE(0x0F, 0x02)    /**< Set all from bitmask    */
#define CMD_ACT_GET_ALL     CMD_CODE(0x0F, 0x03)    /**< Get all as bitmask      */
#define CMD_ACT_CLEAR_ALL   CMD_CODE(0x0F, 0x04)    /**< All actuators OFF       */
#define CMD_ACT_ENABLE      CMD_CODE(0x0F, 0x10)    /**< Enable L293Q drivers    */
#define CMD_ACT_DISABLE     CMD_CODE(0x0F, 0x11)    /**< Disable L293Q drivers   */
#define CMD_ACT_GET_ENABLE  CMD_CODE(0x0F, 0x12)    /**< Get enable state        */
#define CMD_GET_FW_VERSION  CMD_CODE(0x0F, 0x98)    /**< Get firmware version    */

/* Firmware version — update on each release */
#define FW_VERSION_MAJOR    1U
#define FW_VERSION_MINOR    0U
#define FW_VERSION_PATCH    0U

/* Board identity */
#define BOARD_ID_BYTE1      0x41U   /* 'A' = Actuator Board */
#define BOARD_ID_BYTE2      0x42U   /* 'B' */
#define BOARD_ID_SELF       0xFFU   /* Default boardID when talking standalone */

/* ========================= Response Status Codes ========================== */

#define STATUS_OK           0x00U
#define STATUS_ERROR        0x01U
#define STATUS_INVALID_ID   0x02U
#define STATUS_INVALID_VAL  0x03U

/* ========================= Public API ===================================== */

/**
 * @brief  Dispatch an incoming command to the appropriate handler.
 *         Called from the protocol parser callback (ISR context).
 *
 * @param  handle   USART handle for sending responses
 * @param  header   Decoded packet header
 * @param  payload  Payload data (header->length bytes)
 */
void Command_Dispatch(USART_Handle *handle,
                      const PacketHeader *header,
                      const uint8_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_H */
