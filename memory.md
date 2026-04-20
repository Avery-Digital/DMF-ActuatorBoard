# DMF Actuator Board — Project State (Portable Context)

Last updated: 2026-04-20

## Current Firmware Version: v2.0.0

## Hardware
- **MCU:** STM32H735RGV6 (TFBGA68, Cortex-M7)
- **Clock:** HSI 64 MHz (no crystal) → PLL1 480 MHz, PLL2Q 128 MHz
- **Comms:** USART2 PA2/PA3 via LTC2864 RS485 (half-duplex, cross-strapped A↔Y B↔Z)
- **DE:** PA5, **RE:** PA4, **Baud:** 115200

## What It Does
- 28 GPIO half-bridge actuator outputs via 8x L293Q
- Inverse logic: GPIO LOW = ON, GPIO HIGH = OFF
- L293Q enable on PD2 (all 8 chips)
- 0-based actuator IDs (0-27) with switch_map[] logical-to-physical mapping
- SET_ALL/GET_ALL use big-endian 4-byte bitmask

## Command Range: 0x0F00–0x10FF
- CMD_ACT_SET (0x0F00), GET (0x0F01), SET_ALL (0x0F02), GET_ALL (0x0F03), CLEAR_ALL (0x0F04)
- CMD_ACT_ENABLE (0x0F10), DISABLE (0x0F11), GET_ENABLE (0x0F12)
- CMD_GET_FW_VERSION (0x0F98), CMD_GET_BOARD_TYPE (0x0B99)
- All payloads start with [boardID], responses with [status1][status2][boardID]

## Related Projects
- Motherboard routes 0x0F00-0x10FF → actuator boards (boardID 0-1)
- GUI: C:\DMF Board\Software\DMF_DriverBoard_GUI
