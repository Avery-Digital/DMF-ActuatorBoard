/*******************************************************************************
 * @file    Src/Command.c
 * @author  Cam
 * @brief   Command Dispatch and Handlers for DMF Actuator Board
 *
 *          All command handlers run in ISR context.  They populate
 *          tx_request and set .pending = true.  The main loop consumes
 *          the request and calls USART_Driver_SendPacket().
 *
 *          NEVER call USART_Driver_SendPacket() directly from a handler.
 *
 *          Payload format (matches driver board convention):
 *            Incoming:  [boardID] [command-specific data...]
 *            Response:  [status1] [status2] [boardID] [response data...]
 *
 *          This allows the GUI to address any board uniformly, whether
 *          talking directly or through the motherboard.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#include "Command.h"
#include "endian_be.h"
#include "Actuator.h"
#include "main.h"
#include <string.h>

/* ==========================================================================
 *  HELPER — populate tx_request header fields
 * ========================================================================== */
static inline void TxReply(const PacketHeader *header,
                           const uint8_t *data, uint16_t len)
{
    tx_request.msg1 = header->msg1;
    tx_request.msg2 = header->msg2;
    tx_request.cmd1 = header->cmd1;
    tx_request.cmd2 = header->cmd2;
    if (len > 0U && data != NULL) {
        memcpy(tx_request.payload, data, len);
    }
    tx_request.length  = len;
    tx_request.pending = true;
}

/* Extract boardID from payload[0], defaulting to 0xFF if no payload */
static inline uint8_t GetBoardID(const uint8_t *payload, uint16_t length)
{
    return (length > 0U) ? payload[0] : BOARD_ID_SELF;
}

/* ==========================================================================
 *  CMD_PING (0xDEAD)
 * ========================================================================== */
static void Command_HandlePing(const PacketHeader *header)
{
    static const uint8_t ping_response[] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04
    };
    TxReply(header, ping_response, sizeof(ping_response));
}

/* ==========================================================================
 *  CMD_GET_BOARD_TYPE (0x0F99)
 *  Payload in:  [boardID]
 *  Response:    [status1] [status2] [boardID] ['A'] ['B']
 * ========================================================================== */
static void Command_HandleGetBoardType(const PacketHeader *header,
                                       const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    uint8_t r[5] = { STATUS_CAT_OK, STATUS_CODE_OK, bid, BOARD_ID_BYTE1, BOARD_ID_BYTE2 };
    TxReply(header, r, sizeof(r));
}

/* ==========================================================================
 *  CMD_GET_FW_VERSION (0x0F98)
 *  Payload in:  [boardID]
 *  Response:    [status1] [status2] [boardID] [major] [minor] [patch]
 * ========================================================================== */
static void Command_HandleGetFwVersion(const PacketHeader *header,
                                       const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    /* "ACT_BRD vX.Y.Z" — 14 chars, version built from FW_VERSION_* defines */
    uint8_t r[3 + 14];
    r[0] = STATUS_CAT_OK;
    r[1] = STATUS_CODE_OK;
    r[2] = bid;
    r[3]  = 'A';  r[4]  = 'C';  r[5]  = 'T';  r[6]  = '_';
    r[7]  = 'B';  r[8]  = 'R';  r[9]  = 'D';  r[10] = ' ';
    r[11] = 'v';
    r[12] = '0' + FW_VERSION_MAJOR;
    r[13] = '.';
    r[14] = '0' + FW_VERSION_MINOR;
    r[15] = '.';
    r[16] = '0' + FW_VERSION_PATCH;
    TxReply(header, r, sizeof(r));
}

/* ==========================================================================
 *  CMD_ACT_SET (0x0F00)
 *  Payload in:  [boardID] [act_id] [state: 0x01=ON, 0x00=OFF]
 *  Response:    [status1] [status2] [boardID] [act_id] [actual_state]
 * ========================================================================== */
static void Command_HandleActSet(const PacketHeader *header,
                                 const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    uint8_t r[5] = { STATUS_CAT_OK, STATUS_CODE_OK, bid, 0x00, 0x00 };

    if (header->length < 3U) {
        r[0] = STATUS_CAT_ACTUATOR;
        r[1] = STATUS_ACT_SHORT_PL;
        TxReply(header, r, 5);
        return;
    }

    uint8_t act_id = payload[1];
    bool state = (payload[2] != 0x00U);

    Actuator_Status s = Actuator_Set(act_id, state);
    if (s != ACT_OK) {
        r[0] = STATUS_CAT_ACTUATOR;
        r[1] = STATUS_ACT_INVALID_ID;
    }
    r[3] = act_id;

    /* Read back actual state from the GPIO */
    bool actual = false;
    if (s == ACT_OK) {
        Actuator_Get(act_id, &actual);
    }
    r[4] = actual ? 0x01U : 0x00U;

    TxReply(header, r, 5);
}

/* ==========================================================================
 *  CMD_ACT_GET (0x0F01)
 *  Payload in:  [boardID] [act_id]
 *  Response:    [status1] [status2] [boardID] [state: 0x01=ON, 0x00=OFF]
 * ========================================================================== */
static void Command_HandleActGet(const PacketHeader *header,
                                 const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    uint8_t r[4] = { STATUS_CAT_OK, STATUS_CODE_OK, bid, 0x00 };

    if (header->length < 2U) {
        r[0] = STATUS_CAT_ACTUATOR;
        r[1] = STATUS_ACT_SHORT_PL;
        TxReply(header, r, 4);
        return;
    }

    bool state = false;
    Actuator_Status s = Actuator_Get(payload[1], &state);
    if (s != ACT_OK) {
        r[0] = STATUS_CAT_ACTUATOR;
        r[1] = STATUS_ACT_INVALID_ID;
    }
    r[3] = state ? 0x01U : 0x00U;
    TxReply(header, r, 4);
}

/* ==========================================================================
 *  CMD_ACT_SET_ALL (0x0F02)
 *  Payload in:  [boardID] [mask (4 bytes, BE)] — bit 0 = act 0, bit 27 = act 27
 *  Response:    [status1] [status2] [boardID]
 * ========================================================================== */
static void Command_HandleActSetAll(const PacketHeader *header,
                                    const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    uint8_t r[3] = { STATUS_CAT_OK, STATUS_CODE_OK, bid };

    if (header->length < 5U) {
        r[0] = STATUS_CAT_ACTUATOR;
        r[1] = STATUS_ACT_SHORT_PL;
        TxReply(header, r, 3);
        return;
    }

    uint32_t mask = be32_unpack(&payload[1]);

    Actuator_SetAll(mask);
    TxReply(header, r, 3);
}

/* ==========================================================================
 *  CMD_ACT_GET_ALL (0x0F03)
 *  Payload in:  [boardID]
 *  Response:    [status1] [status2] [boardID] [mask (4 bytes, BE)]
 * ========================================================================== */
static void Command_HandleActGetAll(const PacketHeader *header,
                                    const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    uint32_t mask = Actuator_GetAll();
    uint8_t r[7];
    r[0] = STATUS_CAT_OK;
    r[1] = STATUS_CODE_OK;
    r[2] = bid;
    be32_pack(&r[3], mask);
    TxReply(header, r, 7);
}

/* ==========================================================================
 *  CMD_ACT_CLEAR_ALL (0x0F04)
 *  Payload in:  [boardID]
 *  Response:    [status1] [status2] [boardID]
 * ========================================================================== */
static void Command_HandleActClearAll(const PacketHeader *header,
                                      const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    Actuator_ClearAll();
    uint8_t r[3] = { STATUS_CAT_OK, STATUS_CODE_OK, bid };
    TxReply(header, r, 3);
}

/* ==========================================================================
 *  CMD_ACT_ENABLE (0x0F10)
 *  Payload in:  [boardID]
 *  Response:    [status1] [status2] [boardID]
 * ========================================================================== */
static void Command_HandleActEnable(const PacketHeader *header,
                                    const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    Actuator_Enable();
    uint8_t r[3] = { STATUS_CAT_OK, STATUS_CODE_OK, bid };
    TxReply(header, r, 3);
}

/* ==========================================================================
 *  CMD_ACT_DISABLE (0x0F11)
 *  Payload in:  [boardID]
 *  Response:    [status1] [status2] [boardID]
 * ========================================================================== */
static void Command_HandleActDisable(const PacketHeader *header,
                                     const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    Actuator_Disable();
    uint8_t r[3] = { STATUS_CAT_OK, STATUS_CODE_OK, bid };
    TxReply(header, r, 3);
}

/* ==========================================================================
 *  CMD_ACT_GET_ENABLE (0x0F12)
 *  Payload in:  [boardID]
 *  Response:    [status1] [status2] [boardID] [enabled: 0x01 or 0x00]
 * ========================================================================== */
static void Command_HandleActGetEnable(const PacketHeader *header,
                                       const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);
    uint8_t r[4];
    r[0] = STATUS_CAT_OK;
    r[1] = STATUS_CODE_OK;
    r[2] = bid;
    r[3] = Actuator_IsEnabled() ? 0x01U : 0x00U;
    TxReply(header, r, 4);
}

/* ==========================================================================
 *  COMMAND DISPATCH
 * ========================================================================== */

void Command_Dispatch(USART_Handle *handle,
                      const PacketHeader *header,
                      const uint8_t *payload)
{
    (void)handle;

    uint16_t cmd = CMD_CODE(header->cmd1, header->cmd2);

    switch (cmd) {

    /* ---- Shared ---- */
    case CMD_PING:
        Command_HandlePing(header);
        break;

    case CMD_GET_BOARD_TYPE:
        Command_HandleGetBoardType(header, payload);
        break;

    case CMD_GET_FW_VERSION:
        Command_HandleGetFwVersion(header, payload);
        break;

    /* ---- Actuator Control ---- */
    case CMD_ACT_SET:
        Command_HandleActSet(header, payload);
        break;

    case CMD_ACT_GET:
        Command_HandleActGet(header, payload);
        break;

    case CMD_ACT_SET_ALL:
        Command_HandleActSetAll(header, payload);
        break;

    case CMD_ACT_GET_ALL:
        Command_HandleActGetAll(header, payload);
        break;

    case CMD_ACT_CLEAR_ALL:
        Command_HandleActClearAll(header, payload);
        break;

    case CMD_ACT_ENABLE:
        Command_HandleActEnable(header, payload);
        break;

    case CMD_ACT_DISABLE:
        Command_HandleActDisable(header, payload);
        break;

    case CMD_ACT_GET_ENABLE:
        Command_HandleActGetEnable(header, payload);
        break;

    default:
        break;
    }
}
