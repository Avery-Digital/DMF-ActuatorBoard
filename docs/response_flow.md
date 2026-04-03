# Command Response Flow — End-to-End

This document traces exactly how a command travels from the PC to the actuator board and how the response gets back, with every function call and data structure along the way.

## Overview

```
PC (GUI)  ──USB──>  Motherboard  ──RS485──>  Actuator Board
                                                    │
                                              [Process Command]
                                              [Build Response]
                                                    │
PC (GUI)  <──USB──  Motherboard  <──RS485──  Actuator Board
```

The actuator board never talks to the PC directly. All communication is relayed through the motherboard.

---

## Step-by-Step: GUI to Actuator Board (Request)

### 1. GUI Sends Command

The GUI calls `sendActCommand()` which builds a framed packet:

```
[SOF 0x02] [msg1] [msg2] [len_hi] [len_lo] [cmd1] [cmd2] [payload...] [CRC_hi] [CRC_lo] [EOF 0x7E]
```

Example — Enable command for boardID 0:
```
Payload: [0x00]              (boardID = 0, 0-based)
Command: cmd1=0x0F, cmd2=0x10  (CMD_ACT_ENABLE = 0x0F10)
```

The GUI sends this over USB serial to the motherboard at 115200 baud.

### 2. Motherboard Receives and Parses

On the motherboard (DMF-Motherboard-R1):

1. **USART10 DMA RX** receives bytes into circular buffer
2. **DMA HT/TC or USART IDLE ISR** fires → calls `USART_Driver_RxProcessISR()`
3. New bytes fed to `Protocol_FeedBytes()` which runs the frame parser state machine
4. On complete valid frame (CRC check passes): calls `OnPacketReceived()` callback

### 3. Motherboard Routes to Actuator Board

`OnPacketReceived()` calls `Command_Dispatch()` which checks the command code:

```c
// Command.c — default case
if (cmd >= CMD_ACT_RANGE_START && cmd <= CMD_ACT_RANGE_END) {
    Command_HandleActForward(header, payload);
}
```

`CMD_ACT_RANGE_START` = 0x0F00, `CMD_ACT_RANGE_END` = 0x10FF. Since 0x0F10 is in this range, it's forwarded.

`Command_HandleActForward()` runs in **ISR context**. It does NOT send immediately — it populates a deferred request:

```c
act_forward_request.board_id = payload[0];   // boardID from first payload byte
act_forward_request.msg1 = header->msg1;     // echo fields
act_forward_request.msg2 = header->msg2;
act_forward_request.cmd1 = header->cmd1;
act_forward_request.cmd2 = header->cmd2;
memcpy(act_forward_request.payload, payload, header->length);
act_forward_request.length = header->length;
act_forward_request.pending = true;          // commit — main loop will pick this up
```

### 4. Motherboard Main Loop Forwards via RS485

The main loop detects `act_forward_request.pending`:

```c
if (act_forward_request.pending) {
    act_forward_request.pending = false;
    Act_Uart_Handle *act = ACT_GetHandle(act_forward_request.board_id);
    if (act != NULL) {
        Act_Uart_SendPacket(act, ...);  // polled TX with RS485 DE toggling
    }
}
```

`ACT_GetHandle(0)` returns `&act1_handle` (UART5), `ACT_GetHandle(1)` returns `&act2_handle` (USART6). BoardID is 0-based.

`Act_Uart_SendPacket()`:
1. Calls `Protocol_BuildPacket()` to frame the packet (SOF + byte-stuffed header/payload/CRC + EOF)
2. Sets DE pin LOW (through NOT gate → DE=HIGH on LTC2864 = transmit mode)
3. Sends each byte polled via `LL_USART_TransmitData8()`
4. Waits for USART TC (transmission complete)
5. Sets DE pin HIGH (through NOT gate → DE=LOW = receive mode)

The packet travels over the RS485 differential pair to the actuator board's LTC2864 transceiver.

---

## Step-by-Step: Actuator Board Processes Command

### 5. Actuator Board Receives and Parses

On the actuator board (DMF-ActuatorBoard):

1. **USART2 DMA1 Stream 1** (circular mode) receives bytes into `usart2_rx_dma_buf[4096]` in D2 SRAM
2. One of three ISRs fires:
   - **DMA1 Stream 1 HT** — buffer 50% full
   - **DMA1 Stream 1 TC** — buffer wraps
   - **USART2 IDLE** — gap after last byte (most common for single packets)
3. ISR calls `USART_Driver_RxProcessISR(&usart2_handle)`
4. Driver compares DMA write position with `rx_head`, feeds new bytes to `Protocol_FeedBytes()`

### 6. Frame Parser Validates Packet

`Protocol_FeedBytes()` runs the state machine:

```
PARSE_WAIT_SOF  →  [sees 0x02]  →  PARSE_IN_FRAME
PARSE_IN_FRAME  →  [collects bytes, handles ESC]
PARSE_IN_FRAME  →  [sees 0x7E]  →  Validate:
    1. Extract header (msg1, msg2, length, cmd1, cmd2)
    2. Verify received length matches header length field
    3. Compute CRC-16 over header + payload
    4. Compare computed CRC with received CRC
    5. If valid: call on_packet callback → OnPacketReceived()
    6. If invalid: increment packets_err, reset to PARSE_WAIT_SOF
```

### 7. Command Dispatch

`OnPacketReceived()` calls `Command_Dispatch()`:

```c
void Command_Dispatch(USART_Handle *handle,
                      const PacketHeader *header,
                      const uint8_t *payload)
{
    uint16_t cmd = CMD_CODE(header->cmd1, header->cmd2);  // 0x0F10

    switch (cmd) {
    case CMD_ACT_ENABLE:                                    // match!
        Command_HandleActEnable(header, payload);
        break;
    ...
    }
}
```

### 8. Command Handler Executes and Builds Response

```c
static void Command_HandleActEnable(const PacketHeader *header,
                                    const uint8_t *payload)
{
    uint8_t bid = GetBoardID(payload, header->length);  // payload[0] = boardID
    Actuator_Enable();                                   // PD2 → HIGH
    uint8_t r[3] = { STATUS_CAT_OK, STATUS_CAT_OK, bid }; // [0x00][0x00][boardID]
    TxReply(header, r, 3);                               // populate tx_request
}
```

`Actuator_Enable()` sets PD2 HIGH, which enables all 8 L293Q drivers.

`TxReply()` populates the global `tx_request`:

```c
static inline void TxReply(const PacketHeader *header,
                           const uint8_t *data, uint16_t len)
{
    tx_request.msg1 = header->msg1;     // echo msg1 from request
    tx_request.msg2 = header->msg2;     // echo msg2 from request
    tx_request.cmd1 = header->cmd1;     // echo cmd1 (0x0F)
    tx_request.cmd2 = header->cmd2;     // echo cmd2 (0x10)
    memcpy(tx_request.payload, data, len);
    tx_request.length = len;            // 3 bytes
    tx_request.pending = true;          // MUST be last — acts as commit flag
}
```

**CRITICAL:** This runs in ISR context. It NEVER calls `USART_Driver_SendPacket()` directly. It only sets the struct fields and the `pending` flag.

---

## Step-by-Step: Response Back to PC

### 9. Actuator Board Main Loop Sends Response

The main loop in `main.c`:

```c
while (1) {
    if (tx_request.pending) {
        tx_request.pending = false;
        USART_Driver_SendPacket(&usart2_handle,
                                tx_request.msg1, tx_request.msg2,
                                tx_request.cmd1, tx_request.cmd2,
                                tx_request.payload, tx_request.length);
    }
}
```

`USART_Driver_SendPacket()`:
1. Calls `Protocol_BuildPacket()` to build the framed response in `usart2_tx_dma_buf`:
   ```
   [SOF 0x02] [msg1] [msg2] [0x00] [0x03] [0x0F] [0x10] [0x00] [0x00] [boardID] [CRC_hi] [CRC_lo] [EOF 0x7E]
   ```
   (Byte stuffing applied to any special bytes)
2. Sets `tx_busy = true`
3. Asserts RS485 TX mode: `RS485_SetTxMode()` → DE=HIGH, RE=HIGH
4. Configures DMA1 Stream 0 with TX buffer address and length
5. Enables DMA TC interrupt
6. Starts DMA stream → bytes flow from buffer → USART2 TDR → TX pin → LTC2864 → RS485 bus

### 10. Actuator Board DMA TX Complete ISR

When DMA finishes loading all bytes into the USART TDR:

```c
void USART_Driver_TxCompleteISR(USART_Handle *handle)
{
    // Wait for USART shift register to finish the last byte (~87 µs at 115200)
    while (!LL_USART_IsActiveFlag_TC(handle->cfg->peripheral));
    LL_USART_ClearFlag_TC(handle->cfg->peripheral);

    // Switch RS485 back to receive mode
    RS485_SetRxMode();     // DE=LOW, RE=LOW

    handle->tx_busy = false;
}
```

**Why wait for USART TC?** DMA TC means the last byte was transferred to TDR, but it hasn't finished shifting out of the shift register yet. If we switch RS485 to RX mode too early, the last byte gets corrupted on the bus.

### 11. Motherboard Receives Actuator Response

On the motherboard, the actuator board's UART (UART5 or USART6) receives the response:

1. DMA circular RX captures bytes
2. DMA HT/TC or USART IDLE ISR calls `Act_Uart_RxProcessISR()`
3. Bytes fed to the actuator board's protocol parser
4. On valid frame: `OnACT_PacketReceived()` callback fires

### 12. Motherboard Relays Response to GUI

```c
static void OnACT_PacketReceived(const PacketHeader *header,
                                 const uint8_t *payload,
                                 void *ctx)
{
    if (!tx_request.pending) {
        tx_request.msg1 = header->msg1;     // preserved from original GUI request
        tx_request.msg2 = header->msg2;
        tx_request.cmd1 = header->cmd1;     // 0x0F
        tx_request.cmd2 = header->cmd2;     // 0x10
        memcpy(tx_request.payload, payload, header->length);
        tx_request.length = header->length;
        tx_request.pending = true;
    }
}
```

The motherboard's main loop then sends this via USART10 DMA to the PC:

```c
if (tx_request.pending) {
    tx_request.pending = false;
    USART_Driver_SendPacket(&usart10_handle, ...);
}
```

### 13. GUI Receives and Processes Response

The GUI's serial port DataReceived handler:
1. Reads bytes, applies byte unstuffing
2. Validates CRC
3. Extracts command code from bytes [4] and [5] → 0x0F10
4. Switches on command:
   ```csharp
   case (ushort)TFT_CMD.ACT_ENABLE:
       HandleActEnableResponse();
       break;
   ```
5. `HandleActEnableResponse()` checks status2, updates `ACT_BRD_EN_SW_PANEL` color (GREEN=enabled, RED=disabled)

---

## Response Timing

Typical round-trip for a simple command (e.g. ACT_ENABLE):

| Segment | Time | Notes |
|---------|------|-------|
| GUI → Motherboard (USB-UART) | ~1-2 ms | 115200 baud, ~15 byte packet |
| Motherboard ISR + main loop | ~0.1 ms | Deferred forward |
| Motherboard → Actuator (RS485) | ~1-2 ms | 115200 baud, polled TX |
| Actuator ISR + main loop | ~0.1 ms | Process + build response |
| Actuator → Motherboard (RS485) | ~1-2 ms | 115200 baud, DMA TX |
| Motherboard relay → GUI | ~1-2 ms | 115200 baud, DMA TX |
| **Total round-trip** | **~5-10 ms** | |

---

## What Can Go Wrong

### No Response Received

1. **Button not wired** — Designer missing `.Click +=` event handler subscription
2. **Wrong command code** — GUI sends a code the actuator doesn't recognize (silently ignored)
3. **BoardID mismatch** — GUI sends wrong boardID (boardID is 0-based: 0, 1)
4. **RS485 direction stuck** — DE pin not toggling, transceiver stuck in TX or RX mode
5. **DMA TX busy** — `tx_busy` still true from previous send, `SendPacket()` returns `INIT_ERR_DMA`
6. **tx_request overwritten** — A second command arrives before the main loop processes the first response, overwriting `tx_request`

### Corrupted Response

1. **RS485 switched to RX too early** — DMA TC fires before USART finishes shifting last byte; response to wait for USART TC flag
2. **CRC mismatch** — Noise on RS485 bus, packet dropped by parser
3. **Byte stuffing error** — Builder or parser bug with ESC/SOF/EOF bytes in payload

### Response From Wrong Board

1. **OnACT_PacketReceived doesn't check boardID** — it relays whatever the actuator sends. If both actuator boards respond (shouldn't happen since commands are addressed), the first response wins.

---

## Key Data Structures

### tx_request (Actuator Board)

```c
typedef struct {
    volatile bool   pending;        // Set by ISR, cleared by main loop
    uint8_t         msg1, msg2;     // Echoed from request
    uint8_t         cmd1, cmd2;     // Echoed from request
    uint8_t         payload[4096];  // Response data
    uint16_t        length;         // Response payload length
} TxRequest;
```

`pending` MUST be set last — it acts as the commit/publish flag. The main loop reads the other fields only after seeing `pending == true`.

### act_forward_request (Motherboard)

```c
typedef struct {
    volatile bool   pending;
    uint8_t         msg1, msg2, cmd1, cmd2;
    uint8_t         payload[4096];
    uint16_t        length;
    uint8_t         board_id;       // 0 → UART5 (ACT1), 1 → USART6 (ACT2), 0-based
} ActForwardRequest;
```

### ProtocolParser (Both Boards)

```c
typedef struct {
    ParseState      state;          // WAIT_SOF, IN_FRAME, ESCAPED
    uint16_t        rx_index;       // Current position in rx_buf
    uint16_t        expected_len;   // Payload length from header
    uint16_t        crc_rx;         // CRC extracted from packet
    uint8_t         rx_buf[4104];   // header(6) + payload(4096) + CRC(2)
    PacketHeader    header;         // Decoded header fields
    PacketRxCallback on_packet;     // Callback for valid packets
    void            *cb_ctx;        // Callback context
    uint32_t        packets_ok;     // Good packet counter
    uint32_t        packets_err;    // Bad packet counter
} ProtocolParser;
```
