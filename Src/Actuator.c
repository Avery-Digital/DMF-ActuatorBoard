/*******************************************************************************
 * @file    Src/Actuator.c
 * @author  Cam
 * @brief   Actuator Driver — 28 GPIO-driven half-bridges via L293Q
 *
 *          8x L293Q quad half-bridge drivers (32 channels, 28 used).
 *          All L293Q enable pins are tied together on PD2.
 *          Each actuator output is a push-pull GPIO: HIGH = on, LOW = off.
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#include "Actuator.h"

/* ==========================================================================
 *  ACTUATOR PIN MAP
 *
 *  Index = actuator_id - 1  (actuator 1 = index 0, actuator 28 = index 27)
 *
 *  All pins: push-pull output, no pull, very high speed.
 * ========================================================================== */

#define ACT_PIN(_port_letter, _pin_num) { \
    .clk    = LL_AHB4_GRP1_PERIPH_GPIO##_port_letter, \
    .port   = GPIO##_port_letter, \
    .pin    = LL_GPIO_PIN_##_pin_num, \
    .mode   = LL_GPIO_MODE_OUTPUT, \
    .af     = 0U, \
    .speed  = LL_GPIO_SPEED_FREQ_LOW, \
    .pull   = LL_GPIO_PULL_NO, \
    .output = LL_GPIO_OUTPUT_PUSHPULL, \
}

static const PinConfig act_pins[ACT_COUNT] = {
    /* Act  1 — PA6  (pin 25) */ ACT_PIN(A,  6),
    /* Act  2 — PC4  (pin 27) */ ACT_PIN(C,  4),
    /* Act  3 — PC5  (pin 28) */ ACT_PIN(C,  5),
    /* Act  4 — PA7  (pin 26) */ ACT_PIN(A,  7),
    /* Act  5 — PB0  (pin 29) */ ACT_PIN(B,  0),
    /* Act  6 — PB2  (pin 31) */ ACT_PIN(B,  2),
    /* Act  7 — PB10 (pin 32) */ ACT_PIN(B, 10),
    /* Act  8 — PB1  (pin 30) */ ACT_PIN(B,  1),
    /* Act  9 — PB12 (pin 36) */ ACT_PIN(B, 12),
    /* Act 10 — PB14 (pin 38) */ ACT_PIN(B, 14),
    /* Act 11 — PB15 (pin 39) */ ACT_PIN(B, 15),
    /* Act 12 — PB13 (pin 37) */ ACT_PIN(B, 13),
    /* Act 13 — PC7  (pin 41) */ ACT_PIN(C,  7),
    /* Act 14 — PA8  (pin 43) */ ACT_PIN(A,  8),
    /* Act 15 — PC9  (pin 42) */ ACT_PIN(C,  9),
    /* Act 16 — PC6  (pin 40) */ ACT_PIN(C,  6),
    /* Act 17 — PB4  (pin 59) */ ACT_PIN(B,  4),
    /* Act 18 — PB5  (pin 60) */ ACT_PIN(B,  5),
    /* Act 19 — PC11 (pin 55) */ ACT_PIN(C, 11),
    /* Act 20 — PC12 (pin 56) */ ACT_PIN(C, 12),
    /* Act 21 — PB7  (pin 62) */ ACT_PIN(B,  7),
    /* Act 22 — PB9  (pin 65) */ ACT_PIN(B,  9),
    /* Act 23 — PB8  (pin 64) */ ACT_PIN(B,  8),
    /* Act 24 — PB6  (pin 61) */ ACT_PIN(B,  6),
    /* Act 25 — PC1  (pin 14) */ ACT_PIN(C,  1),
    /* Act 26 — PA1  (pin 18) */ ACT_PIN(A,  1),
    /* Act 27 — PA0  (pin 17) */ ACT_PIN(A,  0),
    /* Act 28 — PC0  (pin 13) */ ACT_PIN(C,  0),
};

/* L293Q enable pin — PD2 (pin 57), active HIGH */
static const PinConfig enable_pin = ACT_PIN(D, 2);

/* Track which actuator IDs have valid pin assignments */
static bool act_valid[ACT_COUNT];

/* ==========================================================================
 *  INIT
 * ========================================================================== */

void Actuator_Init(void)
{
    /* Init enable pin first, default LOW (drivers disabled) */
    Pin_Init(&enable_pin);
    LL_GPIO_ResetOutputPin(enable_pin.port, enable_pin.pin);

    /* Init all actuator output pins, default HIGH (off — inverse logic) */
    for (uint8_t i = 0; i < ACT_COUNT; i++) {
        if (act_pins[i].port != NULL) {
            Pin_Init(&act_pins[i]);
            LL_GPIO_SetOutputPin(act_pins[i].port, act_pins[i].pin);
            act_valid[i] = true;
        } else {
            act_valid[i] = false;
        }
    }
}

/* ==========================================================================
 *  ENABLE / DISABLE
 * ========================================================================== */

void Actuator_Enable(void)
{
    LL_GPIO_SetOutputPin(enable_pin.port, enable_pin.pin);
}

void Actuator_Disable(void)
{
    LL_GPIO_ResetOutputPin(enable_pin.port, enable_pin.pin);
}

bool Actuator_IsEnabled(void)
{
    return LL_GPIO_IsOutputPinSet(enable_pin.port, enable_pin.pin);
}

/* ==========================================================================
 *  SINGLE ACTUATOR CONTROL
 * ========================================================================== */

Actuator_Status Actuator_Set(uint8_t act_id, bool state)
{
    if (act_id < 1U || act_id > ACT_COUNT) {
        return ACT_ERR_INVALID_ID;
    }

    uint8_t idx = act_id - 1U;
    if (!act_valid[idx]) {
        return ACT_ERR_INVALID_ID;
    }

    /* Inverse logic: LOW = ON, HIGH = OFF */
    if (state) {
        LL_GPIO_ResetOutputPin(act_pins[idx].port, act_pins[idx].pin);
    } else {
        LL_GPIO_SetOutputPin(act_pins[idx].port, act_pins[idx].pin);
    }

    return ACT_OK;
}

Actuator_Status Actuator_Get(uint8_t act_id, bool *state)
{
    if (act_id < 1U || act_id > ACT_COUNT) {
        return ACT_ERR_INVALID_ID;
    }

    uint8_t idx = act_id - 1U;
    if (!act_valid[idx]) {
        return ACT_ERR_INVALID_ID;
    }

    /* Inverse logic: pin LOW means actuator is ON */
    *state = !LL_GPIO_IsOutputPinSet(act_pins[idx].port, act_pins[idx].pin);
    return ACT_OK;
}

/* ==========================================================================
 *  BULK ACTUATOR CONTROL
 * ========================================================================== */

void Actuator_SetAll(uint32_t mask)
{
    for (uint8_t i = 0; i < ACT_COUNT; i++) {
        if (!act_valid[i]) continue;

        /* Inverse logic: LOW = ON, HIGH = OFF */
        if (mask & (1UL << i)) {
            LL_GPIO_ResetOutputPin(act_pins[i].port, act_pins[i].pin);
        } else {
            LL_GPIO_SetOutputPin(act_pins[i].port, act_pins[i].pin);
        }
    }
}

uint32_t Actuator_GetAll(void)
{
    uint32_t mask = 0;

    for (uint8_t i = 0; i < ACT_COUNT; i++) {
        if (!act_valid[i]) continue;

        /* Inverse logic: pin LOW means actuator is ON */
        if (!LL_GPIO_IsOutputPinSet(act_pins[i].port, act_pins[i].pin)) {
            mask |= (1UL << i);
        }
    }

    return mask;
}

void Actuator_ClearAll(void)
{
    for (uint8_t i = 0; i < ACT_COUNT; i++) {
        if (!act_valid[i]) continue;
        /* Inverse logic: HIGH = OFF */
        LL_GPIO_SetOutputPin(act_pins[i].port, act_pins[i].pin);
    }
}
