/*******************************************************************************
 * @file    Src/Bsp.c
 * @author  Cam
 * @brief   Board Support Package — Hardware Configuration Data
 *
 *          All const configuration structs live here.  This is the ONLY file
 *          that needs to change if the same firmware is ported to a different
 *          PCB with the same MCU.
 *
 *          Target: STM32H735RGV6 (TFBGA68)
 *          Clock:  HSI 64 MHz (no external crystal)
 *          UART:   USART2 PA2/PA3 via LTC2864 RS485 transceiver
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#include "Bsp.h"
#include <stddef.h>

/* ==========================================================================
 *  DMA BUFFERS — D2 SRAM PLACEMENT
 *
 *  STM32H7 DMA1/DMA2 CANNOT access DTCM (0x20000000).  Buffers must be
 *  in D2 SRAM (0x30000000).  The .dma_buffer section is mapped to
 *  RAM_D2 in the linker script.
 *
 *  32-byte alignment for cache coherency.
 * ========================================================================== */

#define USART2_TX_BUF_SIZE      8512U
#define USART2_RX_BUF_SIZE      4096U

__attribute__((section(".dma_buffer"), aligned(32)))
static uint8_t usart2_tx_dma_buf[USART2_TX_BUF_SIZE];

__attribute__((section(".dma_buffer"), aligned(32)))
static uint8_t usart2_rx_dma_buf[USART2_RX_BUF_SIZE];

/* ==========================================================================
 *  PIN INIT HELPER
 * ========================================================================== */

void Pin_Init(const PinConfig *pin)
{
    LL_AHB4_GRP1_EnableClock(pin->clk);

    LL_GPIO_SetPinMode(pin->port, pin->pin, pin->mode);

    if (pin->mode == LL_GPIO_MODE_ALTERNATE) {
        if (pin->pin <= LL_GPIO_PIN_7) {
            LL_GPIO_SetAFPin_0_7(pin->port, pin->pin, pin->af);
        } else {
            LL_GPIO_SetAFPin_8_15(pin->port, pin->pin, pin->af);
        }
    }

    LL_GPIO_SetPinSpeed(pin->port, pin->pin, pin->speed);
    LL_GPIO_SetPinPull(pin->port, pin->pin, pin->pull);
    LL_GPIO_SetPinOutputType(pin->port, pin->pin, pin->output);
}

/* ==========================================================================
 *  CLOCK TREE CONFIGURATION
 *
 *  HSI          = 64 MHz  (internal RC oscillator, no crystal)
 *
 *  PLL1 VCO in  = 64 / 16      =   4 MHz   (range 2–4 MHz)
 *  PLL1 VCO out =  4 × 120     = 480 MHz   (wide range)
 *  PLL1P        = 480 / 1      = 480 MHz → SYSCLK
 *  PLL1Q        = 480 / 2      = 240 MHz
 *
 *  D1CPRE  /1  → 480 MHz  CPU
 *  HPRE    /2  → 240 MHz  AHB
 *  D1PPRE  /2  → 120 MHz  APB3
 *  D2PPRE1 /2  → 120 MHz  APB1
 *  D2PPRE2 /2  → 120 MHz  APB2
 *  D3PPRE  /2  → 120 MHz  APB4
 *
 *  PLL2: 64 / 16 = 4 MHz in, × 64 = 256 MHz VCO
 *        Q /2 = 128 MHz → USART kernel clock
 * ========================================================================== */
const ClockTree_Config sys_clk_config = {

    .hsi_freq_hz        = 64000000UL,

    .voltage_scale      = LL_PWR_REGU_VOLTAGE_SCALE0,
    .flash_latency      = LL_FLASH_LATENCY_4,

    /* ---- PLL1: System clock ---- */
    .pll1 = {
        .divm               = 16U,
        .divn               = 120U,
        .divp               = 1U,
        .divq               = 2U,
        .divr               = 2U,
        .vco_input_range    = LL_RCC_PLLINPUTRANGE_2_4,
        .vco_output_range   = LL_RCC_PLLVCORANGE_WIDE,
        .enable_p           = true,
        .enable_q           = true,
        .enable_r           = false,
    },

    /* ---- PLL2: USART kernel clock ---- */
    .pll2 = {
        .divm               = 16U,
        .divn               = 64U,
        .divp               = 2U,
        .divq               = 2U,
        .divr               = 2U,
        .vco_input_range    = LL_RCC_PLLINPUTRANGE_2_4,
        .vco_output_range   = LL_RCC_PLLVCORANGE_WIDE,
        .enable_p           = false,
        .enable_q           = true,
        .enable_r           = false,
    },

    /* ---- Bus prescalers ---- */
    .prescalers = {
        .d1cpre     = LL_RCC_SYSCLK_DIV_1,
        .hpre       = LL_RCC_AHB_DIV_2,
        .d1ppre     = LL_RCC_APB3_DIV_2,
        .d2ppre1    = LL_RCC_APB1_DIV_2,
        .d2ppre2    = LL_RCC_APB2_DIV_2,
        .d3ppre     = LL_RCC_APB4_DIV_2,
    },

    /* ---- Derived ---- */
    .sysclk_hz  = 480000000UL,
    .ahb_hz     = 240000000UL,
    .apb1_hz    = 120000000UL,
    .apb2_hz    = 120000000UL,
    .pll2q_hz   = 128000000UL,
};

/* ==========================================================================
 *  USART2 CONFIGURATION — PA2 (TX, Pin 19) / PA3 (RX, Pin 20)
 *
 *  USART2 is on APB1.
 *  Kernel clock source: PLL2Q = 128 MHz.
 *  At 128 MHz, baud = 115200 → BRR ≈ 1111 → 0.01% error.
 *  AF7 for both pins per STM32H735 datasheet.
 * ========================================================================== */
const USART_Config usart2_cfg = {

    .tx_pin = {
        .clk        = LL_AHB4_GRP1_PERIPH_GPIOA,
        .port       = GPIOA,
        .pin        = LL_GPIO_PIN_2,
        .mode       = LL_GPIO_MODE_ALTERNATE,
        .af         = LL_GPIO_AF_7,
        .speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH,
        .pull       = LL_GPIO_PULL_NO,
        .output     = LL_GPIO_OUTPUT_PUSHPULL,
    },

    .rx_pin = {
        .clk        = LL_AHB4_GRP1_PERIPH_GPIOA,
        .port       = GPIOA,
        .pin        = LL_GPIO_PIN_3,
        .mode       = LL_GPIO_MODE_ALTERNATE,
        .af         = LL_GPIO_AF_7,
        .speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH,
        .pull       = LL_GPIO_PULL_UP,
        .output     = LL_GPIO_OUTPUT_PUSHPULL,
    },

    .peripheral         = USART2,
    .bus_clk_enable     = LL_APB1_GRP1_PERIPH_USART2,
    .kernel_clk_source  = LL_RCC_USART234578_CLKSOURCE_PLL2Q,
    .prescaler          = LL_USART_PRESCALER_DIV1,
    .baudrate           = 115200U,
    .data_width         = LL_USART_DATAWIDTH_8B,
    .stop_bits          = LL_USART_STOPBITS_1,
    .parity             = LL_USART_PARITY_NONE,
    .direction          = LL_USART_DIRECTION_TX_RX,
    .hw_flow_control    = LL_USART_HWCONTROL_NONE,
    .oversampling       = LL_USART_OVERSAMPLING_16,
    .kernel_clk_hz      = 128000000UL,

    .irqn               = USART2_IRQn,
    .irq_priority       = 5U,
};

/* ==========================================================================
 *  DMA CONFIGURATION FOR USART2
 *
 *  TX: DMA1 Stream 0, DMAMUX request = USART2_TX
 *      Normal mode — fire once per packet, then stop.
 *
 *  RX: DMA1 Stream 1, DMAMUX request = USART2_RX
 *      Circular mode — continuously fills the ring buffer.
 * ========================================================================== */
const DMA_ChannelConfig usart2_dma_tx_cfg = {
    .dma_clk_enable     = LL_AHB1_GRP1_PERIPH_DMA1,
    .dma                = DMA1,
    .stream             = LL_DMA_STREAM_0,
    .request            = LL_DMAMUX1_REQ_USART2_TX,
    .direction          = LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
    .mode               = LL_DMA_MODE_NORMAL,
    .priority           = LL_DMA_PRIORITY_MEDIUM,
    .periph_data_align  = LL_DMA_PDATAALIGN_BYTE,
    .mem_data_align     = LL_DMA_MDATAALIGN_BYTE,
    .periph_inc         = LL_DMA_PERIPH_NOINCREMENT,
    .mem_inc            = LL_DMA_MEMORY_INCREMENT,
    .use_fifo           = false,
    .fifo_threshold     = 0U,
    .irqn               = DMA1_Stream0_IRQn,
    .irq_priority       = 6U,
};

const DMA_ChannelConfig usart2_dma_rx_cfg = {
    .dma_clk_enable     = LL_AHB1_GRP1_PERIPH_DMA1,
    .dma                = DMA1,
    .stream             = LL_DMA_STREAM_1,
    .request            = LL_DMAMUX1_REQ_USART2_RX,
    .direction          = LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
    .mode               = LL_DMA_MODE_CIRCULAR,
    .priority           = LL_DMA_PRIORITY_HIGH,
    .periph_data_align  = LL_DMA_PDATAALIGN_BYTE,
    .mem_data_align     = LL_DMA_MDATAALIGN_BYTE,
    .periph_inc         = LL_DMA_PERIPH_NOINCREMENT,
    .mem_inc            = LL_DMA_MEMORY_INCREMENT,
    .use_fifo           = false,
    .fifo_threshold     = 0U,
    .irqn               = DMA1_Stream1_IRQn,
    .irq_priority       = 4U,
};

/* ==========================================================================
 *  USART2 RUNTIME HANDLE
 * ========================================================================== */
USART_Handle usart2_handle = {
    .cfg            = &usart2_cfg,
    .dma_tx         = &usart2_dma_tx_cfg,
    .dma_rx         = &usart2_dma_rx_cfg,
    .tx_buf         = usart2_tx_dma_buf,
    .tx_buf_size    = USART2_TX_BUF_SIZE,
    .rx_buf         = usart2_rx_dma_buf,
    .rx_buf_size    = USART2_RX_BUF_SIZE,
    .parser         = NULL,
    .tx_len         = 0U,
    .tx_busy        = false,
    .rx_head        = 0U,
    .crc_accumulator = 0U,
};

/* ==========================================================================
 *  RS485 DIRECTION CONTROL — LTC2864
 *
 *  DE: PA5 (Pin 24) — Driver Enable, active HIGH
 *  RE: PA4 (Pin 23) — Receiver Enable, active LOW
 *
 *  Cross-strapped (A↔Y, B↔Z) = half-duplex on single differential pair.
 *  Idle: DE=LOW, RE=LOW (listening).
 *  TX:   DE=HIGH, RE=HIGH (transmitting, receiver disabled).
 * ========================================================================== */
const RS485_Pins rs485_pins = {
    .de_pin = {
        .clk        = LL_AHB4_GRP1_PERIPH_GPIOA,
        .port       = GPIOA,
        .pin        = LL_GPIO_PIN_5,
        .mode       = LL_GPIO_MODE_OUTPUT,
        .af         = 0U,
        .speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH,
        .pull       = LL_GPIO_PULL_DOWN,
        .output     = LL_GPIO_OUTPUT_PUSHPULL,
    },
    .re_pin = {
        .clk        = LL_AHB4_GRP1_PERIPH_GPIOA,
        .port       = GPIOA,
        .pin        = LL_GPIO_PIN_4,
        .mode       = LL_GPIO_MODE_OUTPUT,
        .af         = 0U,
        .speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH,
        .pull       = LL_GPIO_PULL_DOWN,
        .output     = LL_GPIO_OUTPUT_PUSHPULL,
    },
};
