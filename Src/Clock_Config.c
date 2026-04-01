/*******************************************************************************
 * @file    Src/Clock_Config.c
 * @author  Cam
 * @brief   Clock Configuration — MCU Init and Clock Tree Setup
 *
 *          HSI-based clock tree (no external crystal).
 *          HSI = 64 MHz → PLL1 480 MHz SYSCLK, PLL2 128 MHz USART kernel.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#include "Clock_Config.h"

/* ==========================================================================
 *  MCU INITIALIZATION
 *
 *  Power config, flash latency, MPU, NVIC priority grouping.
 *  Call this FIRST before ClockTree_Init().
 * ========================================================================== */
void MCU_Init(void)
{
    /* Check if last reset was caused by WWDG */
    if (LL_RCC_IsActiveFlag_WWDG1RST())
    {
        LL_RCC_ClearResetFlags();
        __NOP();
    }
    NVIC_DisableIRQ(WWDG_IRQn);

    /* Force-clear all DMA interrupt flags from previous session */
    DMA1->LIFCR = 0xFFFFFFFF;
    DMA1->HIFCR = 0xFFFFFFFF;
    DMA2->LIFCR = 0xFFFFFFFF;
    DMA2->HIFCR = 0xFFFFFFFF;

    /* ---- MPU: Background region, deny all unprivileged access ---- */
    LL_MPU_Disable();
    LL_MPU_ConfigRegion(
        LL_MPU_REGION_NUMBER0,
        0x87,
        0x0,
        LL_MPU_REGION_SIZE_4GB      |
        LL_MPU_TEX_LEVEL0           |
        LL_MPU_REGION_NO_ACCESS     |
        LL_MPU_INSTRUCTION_ACCESS_DISABLE |
        LL_MPU_ACCESS_SHAREABLE     |
        LL_MPU_ACCESS_NOT_CACHEABLE |
        LL_MPU_ACCESS_NOT_BUFFERABLE
    );
    LL_MPU_Enable(LL_MPU_CTRL_PRIVILEGED_DEFAULT);

    /* ---- SYSCFG clock ---- */
    LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SYSCFG);

    /* ---- NVIC: 4 bits preemption, 0 bits sub-priority ---- */
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    NVIC_SetPriority(SysTick_IRQn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));

    /* ---- Flash latency ---- */
    LL_FLASH_SetLatency(sys_clk_config.flash_latency);
    while (LL_FLASH_GetLatency() != sys_clk_config.flash_latency);

    /* ---- Power: SMPS + LDO, VOS0 for 480 MHz ---- */
    LL_PWR_ConfigSupply(LL_PWR_SMPS_2V5_SUPPLIES_LDO);
    LL_PWR_SetRegulVoltageScaling(sys_clk_config.voltage_scale);
    while (!LL_PWR_IsActiveFlag_VOS());
}

/* ==========================================================================
 *  CLOCK TREE INITIALIZATION — HSI BASED
 *
 *  Sequence:
 *    1. HSI is already running at 64 MHz on reset (default)
 *    2. Configure & enable PLL1  →  switch SYSCLK
 *    3. Set bus prescalers
 *    4. Configure & enable PLL2
 * ========================================================================== */
void ClockTree_Init(const ClockTree_Config *clk)
{
    uint32_t timeout;

    /* ---- HSI is already enabled and ready at reset ---- */
    /* Ensure it's enabled (should be default) */
    if (!LL_RCC_HSI_IsReady()) {
        LL_RCC_HSI_Enable();
        timeout = 1000000U;
        while ((LL_RCC_HSI_IsReady() != 1U) && (--timeout));
        if (timeout == 0U) Error_Handler(0x01);
    }

    /* ---- PLL1 ---- */
    LL_RCC_PLL1_Disable();
    timeout = 10000U;
    while ((LL_RCC_PLL1_IsReady() != 0U) && (--timeout));
    if (timeout == 0U) Error_Handler(0x02);

    if (clk->pll1.enable_p) LL_RCC_PLL1P_Enable();
    if (clk->pll1.enable_q) LL_RCC_PLL1Q_Enable();
    if (clk->pll1.enable_r) LL_RCC_PLL1R_Enable();

    LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSI);
    LL_RCC_PLL1_SetVCOInputRange(clk->pll1.vco_input_range);
    LL_RCC_PLL1_SetVCOOutputRange(clk->pll1.vco_output_range);
    LL_RCC_PLL1_SetM(clk->pll1.divm);
    LL_RCC_PLL1_SetN(clk->pll1.divn);
    LL_RCC_PLL1_SetP(clk->pll1.divp);
    LL_RCC_PLL1_SetQ(clk->pll1.divq);
    LL_RCC_PLL1_SetR(clk->pll1.divr);

    LL_RCC_PLL1_Enable();
    timeout = 1000000U;
    while ((LL_RCC_PLL1_IsReady() != 1U) && (--timeout));
    if (timeout == 0U) Error_Handler(0x03);

    /* ---- Set intermediate AHB prescaler before switching SYSCLK ---- */
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);

    /* ---- Switch SYSCLK to PLL1P ---- */
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);
    timeout = 50000U;
    while ((LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1) && (--timeout));
    if (timeout == 0U) Error_Handler(0x04);

    /* ---- Wait for clock to stabilize ---- */
    volatile uint32_t stab = 100000U;
    while (stab--);

    /* ---- Bus prescalers (final values) ---- */
    LL_RCC_SetSysPrescaler(clk->prescalers.d1cpre);
    LL_RCC_SetAHBPrescaler(clk->prescalers.hpre);
    LL_RCC_SetAPB3Prescaler(clk->prescalers.d1ppre);
    LL_RCC_SetAPB1Prescaler(clk->prescalers.d2ppre1);
    LL_RCC_SetAPB2Prescaler(clk->prescalers.d2ppre2);
    LL_RCC_SetAPB4Prescaler(clk->prescalers.d3ppre);

    /* ---- SysTick and core clock variable ---- */
    LL_Init1msTick(clk->sysclk_hz);
    LL_SetSystemCoreClock(clk->sysclk_hz);

    /* ---- PLL2: USART kernel clock (128 MHz) ---- */
    LL_RCC_PLL2_Disable();
    timeout = 10000U;
    while ((LL_RCC_PLL2_IsReady() != 0U) && (--timeout));
    if (timeout == 0U) Error_Handler(0x05);

    if (clk->pll2.enable_p) LL_RCC_PLL2P_Enable();
    if (clk->pll2.enable_q) LL_RCC_PLL2Q_Enable();
    if (clk->pll2.enable_r) LL_RCC_PLL2R_Enable();

    LL_RCC_PLL2_SetVCOInputRange(clk->pll2.vco_input_range);
    LL_RCC_PLL2_SetVCOOutputRange(clk->pll2.vco_output_range);
    LL_RCC_PLL2_SetM(clk->pll2.divm);
    LL_RCC_PLL2_SetN(clk->pll2.divn);
    LL_RCC_PLL2_SetP(clk->pll2.divp);
    LL_RCC_PLL2_SetQ(clk->pll2.divq);
    LL_RCC_PLL2_SetR(clk->pll2.divr);

    LL_RCC_PLL2_Enable();
    timeout = 50000U;
    while ((LL_RCC_PLL2_IsReady() != 1U) && (--timeout));
    if (timeout == 0U) Error_Handler(0x06);
}
