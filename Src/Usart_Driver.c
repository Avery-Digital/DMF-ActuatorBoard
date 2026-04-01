/*******************************************************************************
 * @file    Src/Usart_Driver.c
 * @author  Cam
 * @brief   USART Driver — Interrupt-Driven DMA TX/RX with RS485 Direction
 *
 *          RX: Circular DMA with three interrupt sources:
 *            - DMA Half-Transfer (HT) — buffer 50% full
 *            - DMA Transfer-Complete (TC) — buffer 100% full (wraps)
 *            - USART IDLE line — gap after last byte (end of packet)
 *
 *          TX: Normal-mode DMA fires once per packet.  RS485 DE/RE are
 *          asserted before TX start and de-asserted in the TC ISR.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#include "Usart_Driver.h"
#include <string.h>

/* Private prototypes --------------------------------------------------------*/
static void DMA_Stream_Init(const DMA_ChannelConfig *dma);

/* ==========================================================================
 *  RS485 DIRECTION CONTROL — LTC2864
 *
 *  Cross-strapped half-duplex:
 *    Receive:  DE=LOW,  RE=LOW   (driver hi-Z, receiver active)
 *    Transmit: DE=HIGH, RE=HIGH  (driver active, receiver disabled)
 * ========================================================================== */

void RS485_Init_Pins(void)
{
    Pin_Init(&rs485_pins.de_pin);
    Pin_Init(&rs485_pins.re_pin);

    /* Default to receive mode */
    RS485_SetRxMode();
}

void RS485_SetTxMode(void)
{
    LL_GPIO_SetOutputPin(rs485_pins.de_pin.port, rs485_pins.de_pin.pin);
    LL_GPIO_SetOutputPin(rs485_pins.re_pin.port, rs485_pins.re_pin.pin);
}

void RS485_SetRxMode(void)
{
    LL_GPIO_ResetOutputPin(rs485_pins.de_pin.port, rs485_pins.de_pin.pin);
    LL_GPIO_ResetOutputPin(rs485_pins.re_pin.port, rs485_pins.re_pin.pin);
}

/* ==========================================================================
 *  DMA STREAM INITIALIZATION (internal helper)
 * ========================================================================== */
static void DMA_Stream_Init(const DMA_ChannelConfig *dma)
{
    LL_AHB1_GRP1_EnableClock(dma->dma_clk_enable);

    LL_DMA_DisableStream(dma->dma, dma->stream);
    while (LL_DMA_IsEnabledStream(dma->dma, dma->stream));

    LL_DMA_SetPeriphRequest(dma->dma, dma->stream, dma->request);
    LL_DMA_SetDataTransferDirection(dma->dma, dma->stream, dma->direction);
    LL_DMA_SetStreamPriorityLevel(dma->dma, dma->stream, dma->priority);
    LL_DMA_SetMode(dma->dma, dma->stream, dma->mode);
    LL_DMA_SetPeriphSize(dma->dma, dma->stream, dma->periph_data_align);
    LL_DMA_SetMemorySize(dma->dma, dma->stream, dma->mem_data_align);
    LL_DMA_SetPeriphIncMode(dma->dma, dma->stream, dma->periph_inc);
    LL_DMA_SetMemoryIncMode(dma->dma, dma->stream, dma->mem_inc);

    if (dma->use_fifo) {
        LL_DMA_EnableFifoMode(dma->dma, dma->stream);
        LL_DMA_SetFIFOThreshold(dma->dma, dma->stream, dma->fifo_threshold);
    } else {
        LL_DMA_DisableFifoMode(dma->dma, dma->stream);
    }

    NVIC_SetPriority(dma->irqn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
                                         dma->irq_priority, 0));
    NVIC_EnableIRQ(dma->irqn);
}

/* ==========================================================================
 *  USART DRIVER — INIT
 * ========================================================================== */
InitResult USART_Driver_Init(USART_Handle *handle, ProtocolParser *parser)
{
    const USART_Config       *cfg    = handle->cfg;
    const DMA_ChannelConfig  *dma_tx = handle->dma_tx;
    const DMA_ChannelConfig  *dma_rx = handle->dma_rx;

    handle->parser = (void *)parser;

    /* ---- GPIO ---- */
    Pin_Init(&cfg->tx_pin);
    Pin_Init(&cfg->rx_pin);

    /* ---- RS485 direction pins ---- */
    RS485_Init_Pins();

    /* ---- USART peripheral clock ---- */
    LL_APB1_GRP1_EnableClock(cfg->bus_clk_enable);

    /* ---- Kernel clock source ---- */
    LL_RCC_SetUSARTClockSource(cfg->kernel_clk_source);

    /* ---- USART configuration ---- */
    LL_USART_Disable(cfg->peripheral);

    LL_USART_SetPrescaler(cfg->peripheral, cfg->prescaler);
    LL_USART_SetBaudRate(cfg->peripheral,
                         sys_clk_config.pll2q_hz,
                         cfg->prescaler,
                         cfg->oversampling,
                         cfg->baudrate);
    LL_USART_SetDataWidth(cfg->peripheral, cfg->data_width);
    LL_USART_SetStopBitsLength(cfg->peripheral, cfg->stop_bits);
    LL_USART_SetParity(cfg->peripheral, cfg->parity);
    LL_USART_SetTransferDirection(cfg->peripheral, cfg->direction);
    LL_USART_SetHWFlowCtrl(cfg->peripheral, cfg->hw_flow_control);
    LL_USART_SetOverSampling(cfg->peripheral, cfg->oversampling);

    LL_USART_DisableFIFO(cfg->peripheral);
    LL_USART_ConfigAsyncMode(cfg->peripheral);

    /* ---- DMA streams ---- */
    DMA_Stream_Init(dma_tx);
    DMA_Stream_Init(dma_rx);

    /* TX DMA: peripheral address = USART TDR */
    LL_DMA_SetPeriphAddress(dma_tx->dma, dma_tx->stream,
                            LL_USART_DMA_GetRegAddr(cfg->peripheral,
                                                     LL_USART_DMA_REG_DATA_TRANSMIT));

    /* RX DMA: peripheral address = USART RDR */
    LL_DMA_SetPeriphAddress(dma_rx->dma, dma_rx->stream,
                            LL_USART_DMA_GetRegAddr(cfg->peripheral,
                                                     LL_USART_DMA_REG_DATA_RECEIVE));

    /* RX DMA: memory address and buffer length */
    LL_DMA_SetMemoryAddress(dma_rx->dma, dma_rx->stream,
                            (uint32_t)handle->rx_buf);
    LL_DMA_SetDataLength(dma_rx->dma, dma_rx->stream,
                         handle->rx_buf_size);

    /* ---- Enable DMA requests in USART ---- */
    LL_USART_EnableDMAReq_TX(cfg->peripheral);
    LL_USART_EnableDMAReq_RX(cfg->peripheral);

    /* ---- USART IDLE line interrupt ---- */
    LL_USART_EnableIT_IDLE(cfg->peripheral);

    /* ---- USART NVIC ---- */
    NVIC_SetPriority(cfg->irqn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
                                         cfg->irq_priority, 0));
    NVIC_EnableIRQ(cfg->irqn);

    /* ---- Enable USART ---- */
    LL_USART_Enable(cfg->peripheral);

    while (!LL_USART_IsActiveFlag_TEACK(cfg->peripheral));
    while (!LL_USART_IsActiveFlag_REACK(cfg->peripheral));

    handle->rx_head = 0;

    return INIT_OK;
}

/* ==========================================================================
 *  USART DRIVER — START RX (interrupt-driven circular DMA)
 * ========================================================================== */
void USART_Driver_StartRx(USART_Handle *handle)
{
    const DMA_ChannelConfig *rx = handle->dma_rx;

    LL_DMA_EnableIT_HT(rx->dma, rx->stream);
    LL_DMA_EnableIT_TC(rx->dma, rx->stream);
    LL_DMA_EnableIT_TE(rx->dma, rx->stream);

    LL_DMA_EnableStream(rx->dma, rx->stream);
}

/* ==========================================================================
 *  USART DRIVER — RX PROCESS (called from ISR)
 * ========================================================================== */
void USART_Driver_RxProcessISR(USART_Handle *handle)
{
    const DMA_ChannelConfig *rx = handle->dma_rx;
    ProtocolParser *parser = (ProtocolParser *)handle->parser;

    if (parser == NULL) return;

    uint16_t dma_write_pos = handle->rx_buf_size -
        (uint16_t)LL_DMA_GetDataLength(rx->dma, rx->stream);

    uint16_t read_pos = handle->rx_head;

    if (dma_write_pos == read_pos) {
        return;
    }

    if (dma_write_pos > read_pos) {
        Protocol_FeedBytes(parser,
                           &handle->rx_buf[read_pos],
                           dma_write_pos - read_pos);
    } else {
        Protocol_FeedBytes(parser,
                           &handle->rx_buf[read_pos],
                           handle->rx_buf_size - read_pos);

        if (dma_write_pos > 0) {
            Protocol_FeedBytes(parser,
                               &handle->rx_buf[0],
                               dma_write_pos);
        }
    }

    handle->rx_head = dma_write_pos;
}

/* ==========================================================================
 *  USART DRIVER — SEND PACKET (protocol-aware DMA TX with RS485)
 * ========================================================================== */
InitResult USART_Driver_SendPacket(USART_Handle *handle,
                                   uint8_t msg1, uint8_t msg2,
                                   uint8_t cmd1, uint8_t cmd2,
                                   const uint8_t *payload, uint16_t length)
{
    const DMA_ChannelConfig *tx = handle->dma_tx;

    if (handle->tx_busy) {
        return INIT_ERR_DMA;
    }

    uint16_t frame_len = Protocol_BuildPacket(
        handle->tx_buf,
        msg1, msg2, cmd1, cmd2,
        payload, length
    );

    if (frame_len > handle->tx_buf_size) {
        return INIT_ERR_DMA;
    }

    handle->tx_len  = frame_len;
    handle->tx_busy = true;

    /* Assert RS485 transmit mode before starting DMA */
    RS485_SetTxMode();

    LL_DMA_DisableStream(tx->dma, tx->stream);
    while (LL_DMA_IsEnabledStream(tx->dma, tx->stream));

    LL_DMA_SetMemoryAddress(tx->dma, tx->stream, (uint32_t)handle->tx_buf);
    LL_DMA_SetDataLength(tx->dma, tx->stream, frame_len);

    LL_DMA_ClearFlag_TC0(tx->dma);
    LL_DMA_ClearFlag_HT0(tx->dma);
    LL_DMA_ClearFlag_TE0(tx->dma);

    LL_DMA_EnableIT_TC(tx->dma, tx->stream);
    LL_DMA_EnableIT_TE(tx->dma, tx->stream);

    LL_DMA_EnableStream(tx->dma, tx->stream);

    return INIT_OK;
}

/* ==========================================================================
 *  USART DRIVER — RAW TRANSMIT (no protocol framing)
 * ========================================================================== */
InitResult USART_Driver_Transmit(USART_Handle *handle,
                                 const uint8_t *data, uint16_t len)
{
    const DMA_ChannelConfig *tx = handle->dma_tx;

    if (handle->tx_busy) {
        return INIT_ERR_DMA;
    }

    if (len > handle->tx_buf_size) {
        len = handle->tx_buf_size;
    }

    memcpy(handle->tx_buf, data, len);
    handle->tx_len  = len;
    handle->tx_busy = true;

    RS485_SetTxMode();

    LL_DMA_DisableStream(tx->dma, tx->stream);
    while (LL_DMA_IsEnabledStream(tx->dma, tx->stream));

    LL_DMA_SetMemoryAddress(tx->dma, tx->stream, (uint32_t)handle->tx_buf);
    LL_DMA_SetDataLength(tx->dma, tx->stream, len);

    LL_DMA_ClearFlag_TC0(tx->dma);
    LL_DMA_ClearFlag_HT0(tx->dma);
    LL_DMA_ClearFlag_TE0(tx->dma);

    LL_DMA_EnableIT_TC(tx->dma, tx->stream);

    LL_DMA_EnableStream(tx->dma, tx->stream);

    return INIT_OK;
}

/* ==========================================================================
 *  USART DRIVER — TX COMPLETE (called from ISR)
 *
 *  Wait for USART TC (transmission complete) flag before switching RS485
 *  back to receive mode.  DMA TC means the last byte was loaded into TDR,
 *  but it hasn't finished shifting out yet.  We must wait for USART TC
 *  to ensure the stop bit of the last byte has been transmitted.
 * ========================================================================== */
void USART_Driver_TxCompleteISR(USART_Handle *handle)
{
    /* Wait for USART shift register to finish transmitting the last byte.
     * This is a very short spin (~1 byte time = ~87 µs at 115200 baud). */
    while (!LL_USART_IsActiveFlag_TC(handle->cfg->peripheral));
    LL_USART_ClearFlag_TC(handle->cfg->peripheral);

    /* Switch RS485 transceiver back to receive mode */
    RS485_SetRxMode();

    handle->tx_busy = false;
    handle->tx_len  = 0;
}
