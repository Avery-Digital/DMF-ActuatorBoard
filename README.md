# DMF Actuator Board Firmware

Bare-metal firmware for the DMF (Digital Microfluidics) Actuator Board, built on the **STM32H735RGV6** (TFBGA68 package). Drives 28 actuator outputs through 8x L293Q quad half-bridge ICs, communicating with a motherboard over RS485.

**Firmware version:** 1.0.4 (see [CHANGELOG.md](CHANGELOG.md) for release history)
**Board identity:** `0x41 0x42` ("AB" -- Actuator Board)

---

## Hardware Overview

| Parameter          | Value                                     |
|-------------------|-------------------------------------------|
| MCU               | STM32H735RGV6 (Cortex-M7, TFBGA68)       |
| Clock source      | HSI 64 MHz (internal RC, no crystal)       |
| SYSCLK            | 480 MHz (PLL1)                             |
| Flash             | 1024 KB                                    |
| RAM               | DTCM 128 KB, D1 320 KB, D2 32 KB, D3 16 KB |
| Actuator drivers  | 8x L293Q quad half-bridge (28 of 32 used)  |
| Comm interface    | USART2 via LTC2864 RS485 transceiver       |
| Baud rate         | 115200 8N1                                 |
| Voltage scaling   | VOS0 (required for 480 MHz)                |
| Power supply      | SMPS 2.5 V + LDO                           |
| Driver framework  | STM32 LL only (no HAL, no RTOS)            |

---

## Pin Map

### Communication Pins

| Pin  | TFBGA68 | Function      | Config                     |
|------|---------|---------------|----------------------------|
| PA2  | 19      | USART2 TX     | AF7, push-pull, very high speed |
| PA3  | 20      | USART2 RX     | AF7, pull-up, very high speed   |
| PA4  | 23      | RS485 RE      | Output, push-pull, pull-down    |
| PA5  | 24      | RS485 DE      | Output, push-pull, pull-down    |

### L293Q Enable Pin

| Pin  | TFBGA68 | Function       | Notes                      |
|------|---------|----------------|----------------------------|
| PD2  | 57      | L293Q Enable   | Active HIGH, all 8 chips tied together |

### Actuator Output Pins (28 channels)

All pins are push-pull output, no pull, low speed. **Inverse logic:** GPIO LOW = actuator ON, GPIO HIGH = actuator OFF.

| Actuator | GPIO | TFBGA68 | L293Q Chip | Notes           |
|----------|------|---------|------------|-----------------|
| 1        | PA6  | 25      | U1         |                 |
| 2        | PC4  | 27      | U1         |                 |
| 3        | PC5  | 28      | U1         |                 |
| 4        | PA7  | 26      | U1         |                 |
| 5        | PB0  | 29      | U2         |                 |
| 6        | PB2  | 31      | U2         |                 |
| 7        | PB10 | 32      | U2         |                 |
| 8        | PB1  | 30      | U2         |                 |
| 9        | PB12 | 36      | U3         |                 |
| 10       | PB14 | 38      | U3         |                 |
| 11       | PB15 | 39      | U3         |                 |
| 12       | PB13 | 37      | U3         |                 |
| 13       | PC7  | 41      | U4         |                 |
| 14       | PA8  | 43      | U4         |                 |
| 15       | PC9  | 42      | U4         |                 |
| 16       | PC6  | 40      | U4         |                 |
| 17       | PB4  | 59      | U5         |                 |
| 18       | PB5  | 60      | U5         |                 |
| 19       | PC11 | 55      | U5         |                 |
| 20       | PC12 | 56      | U5         |                 |
| 21       | PB7  | 62      | U6         |                 |
| 22       | PB9  | 65      | U6         |                 |
| 23       | PB8  | 64      | U6         |                 |
| 24       | PB6  | 61      | U6         |                 |
| 25       | PC1  | 14      | U7         |                 |
| 26       | PA1  | 18      | U7         |                 |
| 27       | PA0  | 17      | U7         |                 |
| 28       | PC0  | 13      | U7         |                 |

All 28 actuators have GPIO assignments. Actuators are numbered 1--28.

---

## Clock Configuration

The MCU runs from the internal HSI oscillator at 64 MHz. No external crystal is used.

```
HSI = 64 MHz

PLL1:
  Input:  64 / 16 (DIVM) = 4 MHz       (VCO input range 2-4 MHz)
  VCO:   4 x 120 (DIVN)  = 480 MHz     (wide range)
  P:     480 / 1          = 480 MHz  -> SYSCLK
  Q:     480 / 2          = 240 MHz

PLL2:
  Input:  64 / 16 (DIVM) = 4 MHz       (VCO input range 2-4 MHz)
  VCO:   4 x 64  (DIVN)  = 256 MHz     (wide range)
  Q:     256 / 2          = 128 MHz  -> USART kernel clock

Bus prescalers:
  D1CPRE  /1  -> 480 MHz  (CPU core)
  HPRE    /2  -> 240 MHz  (AHB / AXI)
  D1PPRE  /2  -> 120 MHz  (APB3)
  D2PPRE1 /2  -> 120 MHz  (APB1, USART2 bus clock)
  D2PPRE2 /2  -> 120 MHz  (APB2)
  D3PPRE  /2  -> 120 MHz  (APB4)

Flash latency: 4 wait states
Voltage scaling: VOS0 (required for 480 MHz operation)
Power supply: SMPS 2.5V + LDO
```

---

## Communication Protocol

### Physical Layer -- RS485

The board uses an **LTC2864** RS485/RS422 transceiver. The LTC2864 is a full-duplex chip, but it is wired in **half-duplex mode** by cross-strapping:

- **A <-> Y** (driver output connected back to receiver input)
- **B <-> Z** (driver output connected back to receiver input)

This creates a single differential pair for bidirectional communication.

Direction control:
- **Receive (idle):** DE=LOW, RE=LOW -- driver high-impedance, receiver active
- **Transmit:** DE=HIGH, RE=HIGH -- driver active, receiver disabled

The firmware asserts TX mode before starting DMA transmission and returns to RX mode in the DMA TX complete ISR (after waiting for the USART TC flag to ensure the last stop bit has been transmitted).

### Frame Format

```
[SOF] [MSG1] [MSG2] [LEN_HI] [LEN_LO] [CMD1] [CMD2] [PAYLOAD...] [CRC_HI] [CRC_LO] [EOF]
```

| Field      | Size    | Value/Description                                    |
|------------|---------|------------------------------------------------------|
| SOF        | 1 byte  | `0x02` -- Start of Frame                             |
| MSG1       | 1 byte  | Message ID byte 1 (echoed back in responses)         |
| MSG2       | 1 byte  | Message ID byte 2 (echoed back in responses)         |
| LEN_HI     | 1 byte  | Payload length high byte                             |
| LEN_LO     | 1 byte  | Payload length low byte                              |
| CMD1       | 1 byte  | Command code high byte                               |
| CMD2       | 1 byte  | Command code low byte                                |
| PAYLOAD    | 0-4096  | Command-specific data                                |
| CRC_HI     | 1 byte  | CRC-16 high byte                                     |
| CRC_LO     | 1 byte  | CRC-16 low byte                                      |
| EOF        | 1 byte  | `0x7E` -- End of Frame                               |

### Byte Stuffing

If any data byte (between SOF and EOF) equals SOF (`0x02`), EOF (`0x7E`), or ESC (`0x2D`), it is escaped:

1. Transmit `ESC` (`0x2D`)
2. Transmit `byte XOR ESC` (`byte ^ 0x2D`)

The receiver reverses this: on seeing `ESC`, read the next byte and XOR it with `0x2D`.

### CRC-16

- **Algorithm:** CRC-16 CCITT (polynomial `0x1021`)
- **Initial value:** `0xFFFF`
- **Scope:** Computed over the 6-byte header (MSG1, MSG2, LEN_HI, LEN_LO, CMD1, CMD2) plus the payload, before byte stuffing
- **Implementation:** 256-entry lookup table

### Payload Convention

All commands follow a uniform payload layout:

**Request payload:** `[boardID] [command-specific data...]`
- `boardID` is the first byte, 0-based (0, 1, ...; default `0xFF` if omitted)

**Response payload:** `[status1] [status2] [boardID] [response data...]`
- `status1`: status category (`0x00`=STATUS_CAT_OK, `0x01`=STATUS_CAT_ACTUATOR)
- `status2`: status detail (`0x00`=OK, `0x01`=error, `0x02`=invalid ID, `0x03`=invalid value)
- `boardID`: echoed from request

### DMA Reception

Reception uses circular DMA (DMA1 Stream 1) with three interrupt sources that feed bytes into the protocol parser:

1. **DMA Half-Transfer (HT)** -- buffer 50% full
2. **DMA Transfer-Complete (TC)** -- buffer wraps to start
3. **USART IDLE line** -- gap after last byte received (key for low-latency packet detection)

The RX ring buffer is 4096 bytes in D2 SRAM. The driver tracks a `rx_head` position and feeds new bytes to the parser on each interrupt.

### DMA Transmission

Transmission uses normal-mode DMA (DMA1 Stream 0). The TX buffer is 8512 bytes in D2 SRAM -- large enough for a maximally byte-stuffed packet (SOF + 2x(header+payload+CRC) + EOF).

Sequence:
1. `Protocol_BuildPacket()` fills the TX buffer with the framed, byte-stuffed, CRC'd packet
2. RS485 direction is set to TX mode (DE=HIGH, RE=HIGH)
3. DMA stream is configured and enabled
4. On DMA TC interrupt: wait for USART TC flag, then switch RS485 back to RX mode

---

## Command Reference

All commands use the 16-bit code `CMD_CODE(cmd1, cmd2) = (cmd1 << 8) | cmd2`.

### Shared Commands

| Command           | Code     | Request Payload        | Response Payload                         |
|-------------------|----------|------------------------|------------------------------------------|
| Ping              | `0xDEAD` | (none)                 | `DE AD BE EF 01 02 03 04` (8 bytes)     |
| Get Board Type    | `0x0B99` | `[boardID]`            | `[s1] [s2] [boardID] [0x41] [0x42]`     |
| Get FW Version    | `0x0F98` | `[boardID]`            | `[s1] [s2] [boardID] "ACT_BRD v1.0.1"`  |

### Actuator Commands (range 0x0F00 -- 0x0FFF)

| Command             | Code     | Request Payload                  | Response Payload                                 |
|---------------------|----------|----------------------------------|--------------------------------------------------|
| Set Single Actuator | `0x0F00` | `[boardID] [act_id] [state]`     | `[s1] [s2] [boardID] [act_id] [actual_state]`   |
| Get Single Actuator | `0x0F01` | `[boardID] [act_id]`             | `[s1] [s2] [boardID] [state]`                    |
| Set All Actuators   | `0x0F02` | `[boardID] [mask_BE (4 bytes)]`  | `[s1] [s2] [boardID]`                            |
| Get All Actuators   | `0x0F03` | `[boardID]`                      | `[s1] [s2] [boardID] [mask_BE (4 bytes)]`        |
| Clear All Actuators | `0x0F04` | `[boardID]`                      | `[s1] [s2] [boardID]`                            |
| Enable Drivers      | `0x0F10` | `[boardID]`                      | `[s1] [s2] [boardID]`                            |
| Disable Drivers     | `0x0F11` | `[boardID]`                      | `[s1] [s2] [boardID]`                            |
| Get Enable State    | `0x0F12` | `[boardID]`                      | `[s1] [s2] [boardID] [enabled]`                  |

**Field details:**
- `act_id`: Actuator number 1--28
- `state` / `actual_state` / `enabled`: `0x01` = ON/enabled, `0x00` = OFF/disabled
- `mask_BE`: 32-bit big-endian bitmask; bit 0 = actuator 0, bit 27 = actuator 27. MSB sent first.
- `s1`, `s2`: Status bytes (see below)

**Response status codes:**

Status bytes use a categorized format: `[category] [detail]`.

| s1 (Category) | s2 (Detail)  | Name                     | Meaning                                   |
|----------------|-------------|--------------------------|-------------------------------------------|
| `0x00`         | `0x00`      | STATUS_CAT_OK / OK       | Success                                   |
| `0x01`         | `0x01`      | STATUS_CAT_ACTUATOR / ERROR | Generic actuator error (e.g. short payload) |
| `0x01`         | `0x02`      | STATUS_CAT_ACTUATOR / INVALID_ID | Actuator ID out of range or unassigned |
| `0x01`         | `0x03`      | STATUS_CAT_ACTUATOR / INVALID_VAL | Invalid value                        |

Unrecognized commands are silently ignored (no response sent).

### Detailed Packet Examples

For a software engineer integrating with this board, here is the exact byte-level structure for every command. All values are hexadecimal. Byte stuffing is omitted for clarity — in practice, any `0x02`, `0x7E`, or `0x2D` in the payload/CRC bytes must be escaped.

**Notation:**
- `→` = sent to actuator board
- `←` = returned from actuator board
- `[s1]` = status category (`0x00`=OK, `0x01`=actuator error)
- `[s2]` = status detail (`0x00`=OK, `0x01`=error, `0x02`=invalid ID, `0x03`=invalid value)
- `bid` = boardID (echoed from request)
- Fields in `()` are computed: msg IDs are echoed, CRC is over header+payload

---

#### CMD_PING (0xDEAD)

```
→ [02] [m1] [m2] [00] [00] [DE] [AD] [CRC_hi] [CRC_lo] [7E]
          msg IDs    len=0   cmd=DEAD

← [02] [m1] [m2] [00] [08] [DE] [AD] [DE AD BE EF 01 02 03 04] [CRC_hi] [CRC_lo] [7E]
          msg IDs    len=8   cmd=DEAD   fixed 8-byte test payload
```

No payload in request. Response is always the same 8 bytes.

---

#### CMD_GET_BOARD_TYPE (0x0B99)

```
→ [02] [m1] [m2] [00] [01] [0B] [99] [bid] [CRC_hi] [CRC_lo] [7E]
                   len=1              boardID

← [02] [m1] [m2] [00] [05] [0B] [99] [00] [00] [bid] [41] [42] [CRC_hi] [CRC_lo] [7E]
                   len=5               s1   s2   bid   'A'  'B'
```

Board identity is `0x41 0x42` ("AB" = Actuator Board).

---

#### CMD_GET_FW_VERSION (0x0F98)

```
→ [02] [m1] [m2] [00] [01] [0F] [98] [bid] [CRC_hi] [CRC_lo] [7E]

← [02] [m1] [m2] [00] [12] [0F] [98] [00] [00] [bid] [41 43 54 5F 42 52 44 20 76 31 2E 30 2E 31] [CRC] [7E]
                   len=18              s1   s2   bid   "ACT_BRD v1.0.1" (15 ASCII bytes)
```

---

#### CMD_ACT_ENABLE (0x0F10)

```
→ [02] [m1] [m2] [00] [01] [0F] [10] [bid] [CRC_hi] [CRC_lo] [7E]
                   len=1              boardID

← [02] [m1] [m2] [00] [03] [0F] [10] [00] [00] [bid] [CRC_hi] [CRC_lo] [7E]
                   len=3               s1   s2   bid
```

Sets PD2 HIGH → all L293Q drivers enabled. Response confirms success.

---

#### CMD_ACT_DISABLE (0x0F11)

```
→ [02] [m1] [m2] [00] [01] [0F] [11] [bid] [CRC_hi] [CRC_lo] [7E]

← [02] [m1] [m2] [00] [03] [0F] [11] [00] [00] [bid] [CRC_hi] [CRC_lo] [7E]
                   len=3               s1   s2   bid
```

Sets PD2 LOW → all L293Q drivers disabled.

---

#### CMD_ACT_GET_ENABLE (0x0F12)

```
→ [02] [m1] [m2] [00] [01] [0F] [12] [bid] [CRC_hi] [CRC_lo] [7E]

← [02] [m1] [m2] [00] [04] [0F] [12] [00] [00] [bid] [en] [CRC_hi] [CRC_lo] [7E]
                   len=4               s1   s2   bid   enabled (0x01=yes, 0x00=no)
```

---

#### CMD_ACT_SET (0x0F00) — Set Single Actuator

```
→ [02] [m1] [m2] [00] [03] [0F] [00] [bid] [act_id] [state] [CRC_hi] [CRC_lo] [7E]
                   len=3              boardID  0-27    0x01=ON, 0x00=OFF

← [02] [m1] [m2] [00] [05] [0F] [00] [00] [s2] [bid] [act_id] [actual] [CRC_hi] [CRC_lo] [7E]
                   len=5               s1   s2   bid   act_id   actual GPIO state
```

`actual` is read back from the GPIO after setting — confirms the physical state.
`s2` = `0x02` (INVALID_ID) if `act_id` is out of range (not 0-27) or unassigned. In that case `s1` = `0x01` (STATUS_CAT_ACTUATOR).

---

#### CMD_ACT_GET (0x0F01) — Get Single Actuator State

```
→ [02] [m1] [m2] [00] [02] [0F] [01] [bid] [act_id] [CRC_hi] [CRC_lo] [7E]
                   len=2              boardID  0-27

← [02] [m1] [m2] [00] [04] [0F] [01] [00] [s2] [bid] [state] [CRC_hi] [CRC_lo] [7E]
                   len=4               s1   s2   bid   0x01=ON, 0x00=OFF
```

---

#### CMD_ACT_SET_ALL (0x0F02) — Set All Actuators from Bitmask

```
→ [02] [m1] [m2] [00] [05] [0F] [02] [bid] [b3] [b2] [b1] [b0] [CRC_hi] [CRC_lo] [7E]
                   len=5              boardID  32-bit BE bitmask (MSB first)

← [02] [m1] [m2] [00] [03] [0F] [02] [00] [00] [bid] [CRC_hi] [CRC_lo] [7E]
                   len=3               s1   s2   bid
```

Bitmask is big-endian: bit 0 of b0 = actuator 0, bit 3 of b3 = actuator 27. MSB sent first.
Example: turn on actuators 0, 2, 4 → mask = 0x00000015 → b3=0x00, b2=0x00, b1=0x00, b0=0x15.

---

#### CMD_ACT_GET_ALL (0x0F03) — Get All Actuator States

```
→ [02] [m1] [m2] [00] [01] [0F] [03] [bid] [CRC_hi] [CRC_lo] [7E]

← [02] [m1] [m2] [00] [07] [0F] [03] [00] [00] [bid] [b3] [b2] [b1] [b0] [CRC_hi] [CRC_lo] [7E]
                   len=7               s1   s2   bid   32-bit BE bitmask (MSB first)
```

Same bitmask format as SET_ALL.

---

#### CMD_ACT_CLEAR_ALL (0x0F04) — Turn All Actuators OFF

```
→ [02] [m1] [m2] [00] [01] [0F] [04] [bid] [CRC_hi] [CRC_lo] [7E]

← [02] [m1] [m2] [00] [03] [0F] [04] [00] [00] [bid] [CRC_hi] [CRC_lo] [7E]
                   len=3               s1   s2   bid
```

Sets all 28 actuator GPIOs to HIGH (OFF in inverse logic).

---

### Response Flow Documentation

For a detailed trace of how commands travel from the PC through the motherboard to the actuator board and back, see [docs/response_flow.md](docs/response_flow.md).

---

## Boot Sequence

1. **`SystemInit()`** (startup_stm32h735rgvx.s) -- FPU enable, RCC reset to defaults
2. **`MCU_Init()`** -- WWDG disable, DMA flag clear, MPU config (background region), SYSCFG clock, NVIC priority group 4, flash latency 4 WS (with 100k-count timeout, error `0x07`), power supply (SMPS+LDO with 50k-count settle delay, then VOS0 request with 1M-count timeout, error `0x08`). The settle delay and timeouts were added to fix a cold power-on hang where VOS0 was requested before the SMPS+LDO supply had stabilized.
3. **`ClockTree_Init()`** -- HSI already running; configure and enable PLL1 (480 MHz SYSCLK), set bus prescalers, configure and enable PLL2 (128 MHz USART clock)
4. **`LL_Init1msTick()` + `LL_SYSTICK_EnableIT()`** -- 1 ms SysTick interrupt
5. **`Actuator_Init()`** -- Init PD2 enable pin (LOW = disabled), init 28 actuator GPIOs (HIGH = OFF)
6. **`Protocol_ParserInit()`** -- Initialize frame parser state machine, register `OnPacketReceived` callback
7. **`USART_Driver_Init()`** -- Init PA2/PA3 GPIOs (AF7), RS485 DE/RE pins (PA5/PA4), USART2 peripheral (115200 8N1, PLL2Q kernel clock), DMA1 Stream 0 (TX normal) and Stream 1 (RX circular), enable IDLE interrupt, enable USART
8. **`USART_Driver_StartRx()`** -- Enable DMA HT/TC/TE interrupts on RX stream, start circular DMA

Main loop: poll `tx_request.pending` and transmit deferred responses.

---

## File Map

```
DMF-ActuatorBoard/
|-- Inc/
|   |-- main.h              TxRequest struct, top-level includes
|   |-- Actuator.h           Actuator driver API (28 channels)
|   |-- Bsp.h                Board support types, config struct externs
|   |-- Clock_Config.h       MCU_Init / ClockTree_Init prototypes
|   |-- Command.h            Command codes, dispatch prototype
|   |-- Packet_Protocol.h    Frame constants, parser, packet builder
|   |-- Usart_Driver.h       USART/DMA/RS485 driver API
|   |-- crc16.h              CRC-16 CCITT API
|   |-- ll_tick.h            LL_IncTick / LL_GetTick prototypes
|   |-- stm32h7xx_it.h       ISR declarations
|
|-- Src/
|   |-- main.c               Entry point, init sequence, deferred TX loop
|   |-- Actuator.c            Pin map table, GPIO inverse-logic driver
|   |-- Bsp.c                 All const config data, DMA buffers, Pin_Init()
|   |-- Clock_Config.c        MCU_Init (MPU/power/flash), ClockTree_Init (PLLs)
|   |-- Command.c             Command dispatch + all command handlers
|   |-- Packet_Protocol.c     Frame parser state machine, packet builder
|   |-- Usart_Driver.c        USART/DMA init, RX ring processing, TX with RS485
|   |-- crc16.c               CRC-16 lookup table and functions
|   |-- ll_tick.c             SysTick counter (LL_IncTick / LL_GetTick)
|   |-- stm32h7xx_it.c        All ISR handlers (SysTick, HardFault, DMA, USART)
|   |-- system_stm32h7xx.c    CMSIS SystemInit (ST-provided)
|   |-- syscalls.c             Newlib stubs (ST-generated)
|   |-- sysmem.c               Heap implementation (ST-generated)
|
|-- Startup/
|   |-- startup_stm32h735rgvx.s   Vector table and Reset_Handler
|
|-- Drivers/                   STM32H7 LL driver library (CMSIS + LL)
|
|-- STM32H735RGVX_FLASH.ld    Linker script (FLASH boot, .dma_buffer in RAM_D2)
|-- STM32H735RGVX_RAM.ld      Linker script (RAM boot variant)
|-- datasheets/                Reference datasheets
```

---

## Memory Map (Linker Script)

| Region   | Start        | Size   | Usage                           |
|----------|-------------|--------|---------------------------------|
| FLASH    | 0x08000000  | 1024 KB | Code + constants                |
| ITCMRAM  | 0x00000000  | 64 KB  | (unused)                        |
| DTCMRAM  | 0x20000000  | 128 KB | (unused -- DMA cannot access)   |
| RAM_D1   | 0x24000000  | 320 KB | .data, .bss, heap, stack        |
| RAM_D2   | 0x30000000  | 32 KB  | .dma_buffer (TX 8512B, RX 4096B)|
| RAM_D3   | 0x38000000  | 16 KB  | (unused)                        |

Stack grows downward from top of RAM_D1. Minimum heap 512 bytes, minimum stack 1024 bytes.

---

## NVIC Priority Assignment

All priorities use group 4 (4 bits preemption, 0 bits sub-priority).

| IRQ                | Priority | Purpose                        |
|--------------------|----------|--------------------------------|
| DMA1 Stream 1 (RX)| 4        | USART2 RX circular DMA         |
| USART2             | 5        | IDLE line detection             |
| DMA1 Stream 0 (TX)| 6        | USART2 TX DMA complete          |
| SysTick            | 15       | 1 ms tick (lowest priority)     |

---

## Error Codes

`Error_Handler(fault_code)` halts the MCU. Fault codes identify the failure point.

| Code   | Source                          | Description                                  |
|--------|---------------------------------|----------------------------------------------|
| `0x01` | `ClockTree_Init()`             | HSI not ready                                |
| `0x02` | `ClockTree_Init()`             | PLL1 did not disable                         |
| `0x03` | `ClockTree_Init()`             | PLL1 did not lock                            |
| `0x04` | `ClockTree_Init()`             | SYSCLK did not switch to PLL1                |
| `0x05` | `ClockTree_Init()`             | PLL2 did not disable                         |
| `0x06` | `ClockTree_Init()`             | PLL2 did not lock                            |
| `0x07` | `MCU_Init()`                   | Flash latency did not apply (100k timeout)   |
| `0x08` | `MCU_Init()`                   | VOS0 flag not set after power supply settle (1M timeout) |
| `0x11` | `main()` init sequence         | USART driver init failed                     |
