/*******************************************************************************
 * @file    Inc/Packet_Protocol.h
 * @author  Cam
 * @brief   Packet Protocol — Framing, Parsing, and Packet Building
 *
 *          Packet format (after byte-unstuffing):
 *          [SOF] [MSG1] [MSG2] [LEN_HI] [LEN_LO] [CMD1] [CMD2]
 *                [PAYLOAD ...] [CRC_HI] [CRC_LO] [EOF]
 *
 *          Byte stuffing:  If a data byte equals SOF, EOF, or ESC,
 *          transmit ESC followed by (byte ^ ESC).
 *
 *          CRC-16 CCITT is computed over the 6-byte header + payload
 *          (everything between SOF/EOF, after unstuffing, excluding CRC).
 *
 *          This module is transport-agnostic — it knows nothing about
 *          USART, DMA, or any hardware.  Feed it raw bytes from any source.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef PACKET_PROTOCOL_H
#define PACKET_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "crc16.h"

/* =========================== Frame Constants ============================== */

#define FRAME_SOF           0x02U
#define FRAME_EOF           0x7EU
#define FRAME_ESC           0x2DU

#define PKT_HEADER_SIZE     6U
#define PKT_CRC_SIZE        2U
#define PKT_MAX_PAYLOAD     4096U

#define PKT_TX_BUF_SIZE     (1U + (PKT_HEADER_SIZE + PKT_MAX_PAYLOAD + PKT_CRC_SIZE) * 2U + 1U)
#define PKT_RX_BUF_SIZE     (PKT_HEADER_SIZE + PKT_MAX_PAYLOAD + PKT_CRC_SIZE)

/* ============================ Packet Header =============================== */

typedef struct {
    uint8_t     msg1;
    uint8_t     msg2;
    uint16_t    length;
    uint8_t     cmd1;
    uint8_t     cmd2;
} PacketHeader;

/* ========================== Parser State Machine ========================== */

typedef enum {
    PARSE_WAIT_SOF,
    PARSE_IN_FRAME,
    PARSE_ESCAPED,
} ParseState;

typedef void (*PacketRxCallback)(const PacketHeader *header,
                                 const uint8_t *payload,
                                 void *ctx);

typedef struct {
    ParseState      state;
    uint16_t        rx_index;
    uint16_t        expected_len;
    uint16_t        crc_rx;
    uint8_t         crc_bytes;

    uint8_t         rx_buf[PKT_RX_BUF_SIZE];
    PacketHeader    header;

    PacketRxCallback  on_packet;
    void             *cb_ctx;

    uint32_t        packets_ok;
    uint32_t        packets_err;
} ProtocolParser;

/* ============================ Public API ================================== */

void Protocol_ParserInit(ProtocolParser *parser,
                         PacketRxCallback callback,
                         void *ctx);

void Protocol_ParserReset(ProtocolParser *parser);

void Protocol_FeedBytes(ProtocolParser *parser,
                        const uint8_t *data, uint32_t len);

uint16_t Protocol_BuildPacket(uint8_t *tx_buf,
                              uint8_t msg1, uint8_t msg2,
                              uint8_t cmd1, uint8_t cmd2,
                              const uint8_t *payload, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* PACKET_PROTOCOL_H */
