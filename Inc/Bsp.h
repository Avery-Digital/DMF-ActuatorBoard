/*******************************************************************************
 * @file    Inc/Bsp.h
 * @author  Cam
 * @brief   Board Support Package — Type Definitions and Extern Declarations
 *
 *          This header defines the configuration struct types for all
 *          peripherals and declares the const instances that describe
 *          the specific hardware on this board.
 *
 *          Target: STM32H735RGV6 (TFBGA68)
 *          Actuator Board — single UART to motherboard via RS485 (LTC2864)
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef BSP_H
#define BSP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_cortex.h"
#include "stm32h7xx_ll_dma.h"
#include "stm32h7xx_ll_gpio.h"
#include "stm32h7xx_ll_pwr.h"
#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_system.h"
#include "stm32h7xx_ll_usart.h"
#include "stm32h7xx_ll_utils.h"

/* NVIC Priority Group definitions  ------------------------------------------*/
#ifndef NVIC_PRIORITYGROUP_0
#define NVIC_PRIORITYGROUP_0    ((uint32_t)0x00000007)
#define NVIC_PRIORITYGROUP_1    ((uint32_t)0x00000006)
#define NVIC_PRIORITYGROUP_2    ((uint32_t)0x00000005)
#define NVIC_PRIORITYGROUP_3    ((uint32_t)0x00000004)
#define NVIC_PRIORITYGROUP_4    ((uint32_t)0x00000003)
#endif

/* ========================== Return / Status Types ========================= */

typedef enum {
    INIT_OK             = 0x0000U,
    INIT_ERR_CLK        = (1U << 0),
    INIT_ERR_GPIO       = (1U << 1),
    INIT_ERR_USART      = (1U << 2),
    INIT_ERR_DMA        = (1U << 3),
} InitResult;

/* ============================ Pin Configuration =========================== */

typedef struct {
    uint32_t            clk;
    GPIO_TypeDef       *port;
    uint32_t            pin;
    uint32_t            mode;
    uint32_t            af;
    uint32_t            speed;
    uint32_t            pull;
    uint32_t            output;
} PinConfig;

/* =========================== Clock Configuration ========================== */

typedef struct {
    uint32_t            divm;
    uint32_t            divn;
    uint32_t            divp;
    uint32_t            divq;
    uint32_t            divr;
    uint32_t            vco_input_range;
    uint32_t            vco_output_range;
    bool                enable_p;
    bool                enable_q;
    bool                enable_r;
} PLL_Config;

typedef struct {
    uint32_t            d1cpre;
    uint32_t            hpre;
    uint32_t            d1ppre;
    uint32_t            d2ppre1;
    uint32_t            d2ppre2;
    uint32_t            d3ppre;
} BusPrescaler_Config;

typedef struct {
    uint32_t            hsi_freq_hz;
    uint32_t            voltage_scale;
    uint32_t            flash_latency;

    PLL_Config          pll1;
    PLL_Config          pll2;

    BusPrescaler_Config prescalers;

    uint32_t            sysclk_hz;
    uint32_t            ahb_hz;
    uint32_t            apb1_hz;
    uint32_t            apb2_hz;
    uint32_t            pll2q_hz;
} ClockTree_Config;

/* ========================== USART Configuration =========================== */

typedef struct {
    PinConfig           tx_pin;
    PinConfig           rx_pin;

    USART_TypeDef      *peripheral;
    uint32_t            bus_clk_enable;
    uint32_t            kernel_clk_source;
    uint32_t            prescaler;
    uint32_t            baudrate;
    uint32_t            data_width;
    uint32_t            stop_bits;
    uint32_t            parity;
    uint32_t            direction;
    uint32_t            hw_flow_control;
    uint32_t            oversampling;
    uint32_t            kernel_clk_hz;

    IRQn_Type           irqn;
    uint32_t            irq_priority;
} USART_Config;

/* ============================ DMA Configuration =========================== */

typedef struct {
    uint32_t            dma_clk_enable;

    DMA_TypeDef        *dma;
    uint32_t            stream;
    uint32_t            request;
    uint32_t            direction;
    uint32_t            mode;
    uint32_t            priority;

    uint32_t            periph_data_align;
    uint32_t            mem_data_align;

    uint32_t            periph_inc;
    uint32_t            mem_inc;

    bool                use_fifo;
    uint32_t            fifo_threshold;

    IRQn_Type           irqn;
    uint32_t            irq_priority;
} DMA_ChannelConfig;

/* ====================== USART Runtime Handle ============================== */

typedef struct {
    const USART_Config        *cfg;
    const DMA_ChannelConfig   *dma_tx;
    const DMA_ChannelConfig   *dma_rx;

    uint8_t                   *tx_buf;
    uint16_t                   tx_buf_size;
    uint8_t                   *rx_buf;
    uint16_t                   rx_buf_size;

    void                      *parser;

    volatile uint16_t          tx_len;
    volatile bool              tx_busy;
    volatile uint16_t          rx_head;

    uint16_t                   crc_accumulator;
} USART_Handle;

/* ====================== RS485 Direction Control =========================== */

/**
 * @brief  RS485 transceiver config for LTC2864 (full-duplex chip,
 *         wired half-duplex with A↔Y, B↔Z cross-strapped).
 *
 *         DE (PA5, pin 24): Driver Enable — HIGH to transmit
 *         RE (PA4, pin 23): Receiver Enable — LOW to receive (active low)
 *
 *         Idle state:  DE=LOW, RE=LOW  (listening)
 *         TX state:    DE=HIGH, RE=HIGH (transmitting, RX disabled)
 */
typedef struct {
    PinConfig       de_pin;     /**< DE — Driver Enable (PA5)     */
    PinConfig       re_pin;     /**< RE — Receiver Enable (PA4)   */
} RS485_Pins;

/* ======================== Utility Prototypes ============================== */

void Pin_Init(const PinConfig *pin);

/* ======================= Extern Config Instances ========================== */

/* Clock tree (HSI-based, no external crystal) */
extern const ClockTree_Config   sys_clk_config;

/* USART2 on PA2 (TX) / PA3 (RX) — to motherboard via LTC2864 RS485 */
extern const USART_Config       usart2_cfg;
extern const DMA_ChannelConfig  usart2_dma_tx_cfg;
extern const DMA_ChannelConfig  usart2_dma_rx_cfg;

/* USART2 runtime handle */
extern USART_Handle             usart2_handle;

/* RS485 direction control pins (LTC2864) */
extern const RS485_Pins         rs485_pins;

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
