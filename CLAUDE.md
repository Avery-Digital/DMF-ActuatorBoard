# CLAUDE.md -- Project Instructions for Claude Code

## Project Overview

**DMF Actuator Board Firmware** -- bare-metal STM32H735RGV6 (TFBGA68 package) firmware that drives 28 actuator outputs through 8x L293Q quad half-bridge ICs. Communicates with a motherboard over a single RS485 UART link (USART2 via LTC2864 transceiver).

- **No HAL, no RTOS** -- uses only STM32 LL (Low-Layer) drivers
- **Language:** C (C11)
- **Toolchain:** arm-none-eabi-gcc via STM32CubeIDE
- **Clock source:** HSI 64 MHz internal RC oscillator (no external crystal)

## Build Instructions

1. Open STM32CubeIDE
2. Import the project from `C:\STM32_Firmware\DMF-ActuatorBoard`
3. Build target: `Debug` (output in `Debug/`)
4. Flash via ST-Link SWD

The project uses the STM32H7 LL driver pack under `Drivers/`.

## Architecture Rules

### ISR-Safe Deferred TX

Command handlers run in ISR context (called from DMA HT/TC or USART IDLE interrupts). They **must not** call `USART_Driver_SendPacket()` directly. Instead:

1. Handler populates the global `tx_request` struct (msg1, msg2, cmd1, cmd2, payload, length)
2. Handler sets `tx_request.pending = true`
3. The main `while(1)` loop polls `tx_request.pending` and calls `USART_Driver_SendPacket()` from thread context

This pattern avoids DMA conflicts and keeps ISR execution time short.

### DMA Buffers in D2 SRAM

STM32H7 DMA1/DMA2 **cannot** access DTCM (0x20000000). All DMA buffers must be placed in D2 SRAM (0x30000000) using:

```c
__attribute__((section(".dma_buffer"), aligned(32)))
```

The `.dma_buffer` section is mapped to `RAM_D2` in the linker script (`STM32H735RGVX_FLASH.ld`). Buffers are 32-byte aligned for cache coherency.

### Inverse Logic Actuators

The L293Q half-bridge outputs use **inverse logic** at the GPIO level:
- **GPIO LOW = Actuator ON** (current flows)
- **GPIO HIGH = Actuator OFF** (no current)

The `Actuator_Set()` / `Actuator_SetAll()` API abstracts this -- callers pass `true` for ON, `false` for OFF. The inversion happens inside `Actuator.c`.

At init, all GPIOs are set HIGH (OFF) and the L293Q enable pin (PD2) is set LOW (drivers disabled).

### Naming and Coding Conventions

- Config structs are `const` and live in `Bsp.c` -- this is the only file that changes for a PCB revision
- All LL driver includes go through `Bsp.h`
- ISR handlers live in `stm32h7xx_it.c`; they clear flags and delegate to driver callbacks
- Command handlers live in `Command.c` with `static` linkage; only `Command_Dispatch()` is public
- Status bytes use categorized format: `[category][code]`. Success = `0x00 0x00`. Actuator errors = category `0x07`.
- Actuator IDs are **0-based** (0–27). ID 0 = actuator 1, ID 27 = actuator 28.

## Pin Assignments Summary

| Function         | Pin   | TFBGA68 | Notes                          |
|-----------------|-------|---------|--------------------------------|
| USART2 TX       | PA2   | 19      | AF7, to LTC2864               |
| USART2 RX       | PA3   | 20      | AF7, from LTC2864, pull-up    |
| RS485 RE        | PA4   | 23      | Active LOW (LTC2864)           |
| RS485 DE        | PA5   | 24      | Active HIGH (LTC2864)          |
| L293Q Enable    | PD2   | 57      | All 8 chips, active HIGH       |
| Actuators 1-28  | Various | --    | See Actuator.c pin map         |

## Command Range

**0x0F00 -- 0x10FF** is reserved for the actuator board.

Currently defined commands: `0x0F00`--`0x0F12`, plus shared commands `0xDEAD` (ping) and `0x0B99` (board type).

Commands are 16-bit: `CMD_CODE(cmd1, cmd2) = (cmd1 << 8) | cmd2`.

## Communication

- **Physical:** Single UART (USART2) via LTC2864 RS485 transceiver
- **Topology:** Half-duplex (LTC2864 is full-duplex chip, cross-strapped A-Y B-Z for single differential pair)
- **Baud rate:** 115200, 8N1
- **RS485 direction:** DE=HIGH + RE=HIGH to transmit; DE=LOW + RE=LOW to receive
- **USART kernel clock:** PLL2Q = 128 MHz (gives 0.01% baud error)
- **RX:** Circular DMA (DMA1 Stream 1) with HT/TC/IDLE interrupts feeding protocol parser
- **TX:** Normal DMA (DMA1 Stream 0) with RS485 direction toggling in TC ISR

## Clock Configuration

- **HSI:** 64 MHz (internal, no crystal)
- **PLL1:** 64/16 x 120 = 480 MHz SYSCLK (VOS0, flash latency 4)
- **PLL2Q:** 64/16 x 64 / 2 = 128 MHz USART kernel clock
- **AHB:** 240 MHz (HPRE /2)
- **APB1/APB2/APB3/APB4:** 120 MHz (all /2)

## Error Handling

`Error_Handler(fault_code)` disables interrupts, writes the fault code to RTC backup register DR0, then spins forever. Fault codes:
- `0x01` -- HSI not ready
- `0x02` -- PLL1 disable timeout
- `0x03` -- PLL1 lock timeout
- `0x04` -- SYSCLK switch timeout
- `0x05` -- PLL2 disable timeout
- `0x06` -- PLL2 lock timeout
- `0x11` -- USART driver init failure
