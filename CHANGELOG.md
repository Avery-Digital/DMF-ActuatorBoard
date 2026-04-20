# Changelog

## v2.0.0 — 2026-04-20

### Unified big-endian wire format (refactor only — no behavior change)

- Added `Inc/endian_be.h` shared helpers (`be16_pack/unpack`, `be32_pack/unpack`).
- `CMD_ACT_SET_ALL` / `CMD_ACT_GET_ALL` 32-bit bitmask pack/unpack now use `be32_pack`/`be32_unpack`. The wire format was already big-endian in v1.0.4 — this is a code cleanup, not a protocol change.
- Firmware version string: `"ACT_BRD v2.0.0"`.
- Paired release: every DMF firmware image and the GUI moves to v2.0.0 simultaneously. This bump is a lockstep version, not a behavior change on this board specifically.

---

## v1.0.4 — 2026-04-08

### Big-Endian Bitmask for SET_ALL / GET_ALL
- CMD_ACT_SET_ALL (0x0F02) and CMD_ACT_GET_ALL (0x0F03) now use big-endian byte order for the 4-byte bitmask
- Previously little-endian: `[b0][b1][b2][b3]` (LSB first)
- Now big-endian: `[b3][b2][b1][b0]` (MSB first)
- Example: mask `0x00000009` (actuators 0 and 3) → sent as `[0x00][0x00][0x00][0x09]`
- GUI HandleActGetAllResponse updated to match

---

## v1.0.3 — 2026-04-06

### Logical-to-Physical Switch Mapping
- Added `switch_map[28]` lookup table in `Actuator.c`
- GUI sends logical switch ID (0–27), firmware maps to physical actuator pin
- Mapping follows groups of 4: `[first, first+3, first+2, first+1]` per L293Q chip
- All functions (`Set`, `Get`, `SetAll`, `GetAll`) route through the mapping table
- No changes needed on GUI or motherboard side — mapping is firmware-only

| Logical ID | Physical Act | Logical ID | Physical Act |
|------------|-------------|------------|-------------|
| 0 | Act 1 | 14 | Act 15 |
| 1 | Act 4 | 15 | Act 14 |
| 2 | Act 3 | 16 | Act 17 |
| 3 | Act 2 | 17 | Act 20 |
| 4 | Act 5 | 18 | Act 19 |
| 5 | Act 8 | 19 | Act 18 |
| 6 | Act 7 | 20 | Act 21 |
| 7 | Act 6 | 21 | Act 24 |
| 8 | Act 9 | 22 | Act 23 |
| 9 | Act 12 | 23 | Act 22 |
| 10 | Act 11 | 24 | Act 25 |
| 11 | Act 10 | 25 | Act 28 |
| 12 | Act 13 | 26 | Act 27 |
| 13 | Act 16 | 27 | Act 26 |

---

## v1.0.2 — 2026-04-06

### 0-Based Actuator IDs
- Actuator IDs are now 0-based (0–27) instead of 1-based (1–28)
- Sending `0x00` activates actuator 1, `0x1B` (27) activates actuator 28
- `Actuator_Set()` and `Actuator_Get()` accept 0-based index directly (no offset)
- GUI sends `buttonNumber - 1`, firmware uses ID as direct array index

---

## v1.0.1 — 2026-04-03

### Unified Status Bytes
- All response status codes now use categorized format: `[category][code]`
- Replaced ad-hoc STATUS_OK/STATUS_ERROR/STATUS_INVALID_ID/STATUS_INVALID_VAL with:
  - `STATUS_CAT_OK (0x00)` + `STATUS_CODE_OK (0x00)` = success
  - `STATUS_CAT_GENERAL (0x01)` + specific code = general errors
  - `STATUS_CAT_ACTUATOR (0x07)` + specific code = actuator-specific errors
- Error codes: 0x07/0x01 = invalid actuator ID, 0x07/0x02 = invalid value, 0x07/0x03 = payload too short

### Pin Assignments
- Actuator 26 assigned to PA1 (pin 18) — was previously unassigned
- Actuator 27 assigned to PA0 (pin 17) — was previously unassigned (was floating HIGH, causing false-on)

### Cold Boot Fix
- Increased SMPS+LDO settle delay from 50K to 1M loop iterations (~50 ms at 64 MHz HSI)
- Increased VOS0 flag timeout from 1M to 5M iterations
- Increased SYSCLK switch timeout from 50K to 1M iterations
- Increased post-SYSCLK-switch stabilization delay from 100K to 1M iterations (~2 ms at 480 MHz)
- Added error codes 0x07 (flash latency timeout) and 0x08 (VOS0 timeout) to MCU_Init

### BoardID Convention
- Actuator board is now boardID-agnostic — it echoes whatever boardID is in payload[0]
- Motherboard changed from 1-based (1, 2) to 0-based (0, 1) routing

### Status Code Table (Actuator Board)

| status1 | status2 | Meaning |
|---------|---------|---------|
| 0x00 | 0x00 | Success |
| 0x01 | 0x01 | Payload too short (general) |
| 0x07 | 0x01 | Invalid actuator ID (not 0-27) |
| 0x07 | 0x02 | Invalid actuator value |
| 0x07 | 0x03 | Payload too short for command |

---

## v1.0.0 — 2026-04-01

- Initial release: 28 actuator GPIO outputs via 8x L293Q
- USART2 RS485 communication via LTC2864
- Binary packet protocol with CRC-16 CCITT
- Commands: Set/Get/SetAll/GetAll/ClearAll/Enable/Disable/GetEnable
