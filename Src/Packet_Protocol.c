/*******************************************************************************
 * @file    Src/Packet_Protocol.c
 * @author  Cam
 * @brief   Packet Protocol — Framing, Parsing, and Packet Building
 *
 *          Transport-agnostic implementation.  Operates on raw byte streams
 *          without any knowledge of USART, DMA, or hardware registers.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#include "Packet_Protocol.h"
#include <stddef.h>

/* ==========================================================================
 *  PRIVATE HELPERS
 * ========================================================================== */

static void AppendEscaped(uint8_t *buf, uint16_t *idx, uint8_t b)
{
    if (b == FRAME_SOF || b == FRAME_EOF || b == FRAME_ESC) {
        buf[(*idx)++] = FRAME_ESC;
        buf[(*idx)++] = b ^ FRAME_ESC;
    } else {
        buf[(*idx)++] = b;
    }
}

static void DecodeHeader(const uint8_t *buf, PacketHeader *hdr)
{
    hdr->msg1   = buf[0];
    hdr->msg2   = buf[1];
    hdr->length = ((uint16_t)buf[2] << 8) | buf[3];
    hdr->cmd1   = buf[4];
    hdr->cmd2   = buf[5];
}

/* ==========================================================================
 *  PARSER INIT / RESET
 * ========================================================================== */

void Protocol_ParserInit(ProtocolParser *parser,
                         PacketRxCallback callback,
                         void *ctx)
{
    parser->state        = PARSE_WAIT_SOF;
    parser->rx_index     = 0;
    parser->expected_len = 0;
    parser->crc_rx       = 0;
    parser->crc_bytes    = 0;
    parser->on_packet    = callback;
    parser->cb_ctx       = ctx;
    parser->packets_ok   = 0;
    parser->packets_err  = 0;
}

void Protocol_ParserReset(ProtocolParser *parser)
{
    parser->state        = PARSE_WAIT_SOF;
    parser->rx_index     = 0;
    parser->expected_len = 0;
    parser->crc_rx       = 0;
    parser->crc_bytes    = 0;
}

/* ==========================================================================
 *  PARSER — FEED BYTES
 * ========================================================================== */

void Protocol_FeedBytes(ProtocolParser *parser,
                        const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        bool store = false;

        switch (parser->state) {

        case PARSE_WAIT_SOF:
            if (b == FRAME_SOF) {
                parser->rx_index     = 0;
                parser->expected_len = 0;
                parser->crc_rx       = 0;
                parser->crc_bytes    = 0;
                parser->state        = PARSE_IN_FRAME;
            }
            continue;

        case PARSE_IN_FRAME:
            if (b == FRAME_ESC) {
                parser->state = PARSE_ESCAPED;
                continue;
            }

            if (b == FRAME_SOF) {
                parser->rx_index     = 0;
                parser->expected_len = 0;
                parser->crc_rx       = 0;
                parser->crc_bytes    = 0;
                parser->packets_err++;
                continue;
            }

            if (b == FRAME_EOF) {
                uint16_t total_data = PKT_HEADER_SIZE + parser->expected_len;

                if (parser->rx_index >= total_data &&
                    parser->crc_bytes == PKT_CRC_SIZE)
                {
                    uint16_t crc_calc = CRC16_Calc(parser->rx_buf, total_data);

                    if (crc_calc == parser->crc_rx) {
                        DecodeHeader(parser->rx_buf, &parser->header);
                        parser->packets_ok++;

                        if (parser->on_packet != NULL) {
                            parser->on_packet(
                                &parser->header,
                                &parser->rx_buf[PKT_HEADER_SIZE],
                                parser->cb_ctx
                            );
                        }
                    } else {
                        parser->packets_err++;
                    }
                } else {
                    parser->packets_err++;
                }

                parser->state = PARSE_WAIT_SOF;
                continue;
            }

            store = true;
            break;

        case PARSE_ESCAPED:
            b ^= FRAME_ESC;
            parser->state = PARSE_IN_FRAME;
            store = true;
            break;
        }

        if (!store) {
            continue;
        }

        uint16_t data_end = PKT_HEADER_SIZE + parser->expected_len;

        if (parser->rx_index < PKT_HEADER_SIZE) {
            if (parser->rx_index < PKT_RX_BUF_SIZE) {
                parser->rx_buf[parser->rx_index] = b;
            }

            if (parser->rx_index == 3) {
                parser->expected_len =
                    ((uint16_t)parser->rx_buf[2] << 8) | b;

                if (parser->expected_len > PKT_MAX_PAYLOAD) {
                    parser->state = PARSE_WAIT_SOF;
                    parser->packets_err++;
                    continue;
                }
            }
        }
        else if (parser->rx_index < data_end) {
            if (parser->rx_index < PKT_RX_BUF_SIZE) {
                parser->rx_buf[parser->rx_index] = b;
            }
        }
        else if (parser->crc_bytes < PKT_CRC_SIZE) {
            parser->crc_rx = (parser->crc_rx << 8) | b;
            parser->crc_bytes++;
        }
        else {
            parser->state = PARSE_WAIT_SOF;
            parser->packets_err++;
            continue;
        }

        parser->rx_index++;
    }
}

/* ==========================================================================
 *  BUILD PACKET
 * ========================================================================== */

uint16_t Protocol_BuildPacket(uint8_t *tx_buf,
                              uint8_t msg1, uint8_t msg2,
                              uint8_t cmd1, uint8_t cmd2,
                              const uint8_t *payload, uint16_t length)
{
    uint16_t idx = 0;

    if (length > PKT_MAX_PAYLOAD) {
        length = PKT_MAX_PAYLOAD;
    }

    uint16_t crc = CRC16_INIT;
    crc = CRC16_Update(crc, msg1);
    crc = CRC16_Update(crc, msg2);
    crc = CRC16_Update(crc, (length >> 8) & 0xFF);
    crc = CRC16_Update(crc, length & 0xFF);
    crc = CRC16_Update(crc, cmd1);
    crc = CRC16_Update(crc, cmd2);

    for (uint16_t i = 0; i < length; i++) {
        crc = CRC16_Update(crc, payload[i]);
    }

    tx_buf[idx++] = FRAME_SOF;

    AppendEscaped(tx_buf, &idx, msg1);
    AppendEscaped(tx_buf, &idx, msg2);
    AppendEscaped(tx_buf, &idx, (length >> 8) & 0xFF);
    AppendEscaped(tx_buf, &idx, length & 0xFF);
    AppendEscaped(tx_buf, &idx, cmd1);
    AppendEscaped(tx_buf, &idx, cmd2);

    for (uint16_t i = 0; i < length; i++) {
        AppendEscaped(tx_buf, &idx, payload[i]);
    }

    AppendEscaped(tx_buf, &idx, (crc >> 8) & 0xFF);
    AppendEscaped(tx_buf, &idx, crc & 0xFF);

    tx_buf[idx++] = FRAME_EOF;

    return idx;
}
