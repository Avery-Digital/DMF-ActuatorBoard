/*******************************************************************************
 * @file    Inc/Actuator.h
 * @author  Cam
 * @brief   Actuator Driver — 28 GPIO-driven half-bridges via L293Q
 *
 *          8x L293Q quad half-bridge drivers, all enables tied to PD2.
 *          Each actuator output is driven by a single GPIO pin (HIGH = on).
 *
 *          Actuator numbering: 1–28 (array index = actuator_id - 1)
 *******************************************************************************
 * Copyright (c) 2026
 * All rights reserved.
 ******************************************************************************/

#ifndef ACTUATOR_H
#define ACTUATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Bsp.h"

/* =========================== Constants ==================================== */

#define ACT_COUNT           28U     /**< Total number of actuator outputs     */

/* =========================== Status Codes ================================= */

typedef enum {
    ACT_OK              = 0x00U,
    ACT_ERR_INVALID_ID  = 0x01U,
    ACT_ERR_INVALID_VAL = 0x02U,
} Actuator_Status;

/* =========================== Public API =================================== */

/**
 * @brief  Initialise all actuator GPIO pins and the L293Q enable pin.
 *         All outputs default OFF, enable defaults OFF (drivers disabled).
 */
void Actuator_Init(void);

/**
 * @brief  Enable the L293Q drivers (PD2 HIGH).
 *         Outputs will reflect their GPIO state.
 */
void Actuator_Enable(void);

/**
 * @brief  Disable the L293Q drivers (PD2 LOW).
 *         All outputs go to high-impedance regardless of GPIO state.
 */
void Actuator_Disable(void);

/**
 * @brief  Check if drivers are enabled.
 * @return true if PD2 is HIGH
 */
bool Actuator_IsEnabled(void);

/**
 * @brief  Set a single actuator ON or OFF.
 * @param  act_id  Actuator number (1–28)
 * @param  state   true = ON, false = OFF
 * @return ACT_OK or ACT_ERR_INVALID_ID
 */
Actuator_Status Actuator_Set(uint8_t act_id, bool state);

/**
 * @brief  Get the current state of a single actuator.
 * @param  act_id  Actuator number (1–28)
 * @param  state   Output: true = ON, false = OFF
 * @return ACT_OK or ACT_ERR_INVALID_ID
 */
Actuator_Status Actuator_Get(uint8_t act_id, bool *state);

/**
 * @brief  Set all 28 actuators from a 32-bit bitmask.
 *         Bit 0 = actuator 1, bit 27 = actuator 28.
 * @param  mask  Bitmask of actuator states (1 = ON, 0 = OFF)
 */
void Actuator_SetAll(uint32_t mask);

/**
 * @brief  Get all 28 actuator states as a 32-bit bitmask.
 *         Bit 0 = actuator 1, bit 27 = actuator 28.
 * @return Bitmask of current actuator states
 */
uint32_t Actuator_GetAll(void);

/**
 * @brief  Turn all actuators OFF.
 */
void Actuator_ClearAll(void);

#ifdef __cplusplus
}
#endif

#endif /* ACTUATOR_H */
