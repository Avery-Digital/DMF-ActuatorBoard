# Changelog

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
| 0x07 | 0x01 | Invalid actuator ID (not 1-28) |
| 0x07 | 0x02 | Invalid actuator value |
| 0x07 | 0x03 | Payload too short for command |

---

## v1.0.0 — 2026-04-01

- Initial release: 28 actuator GPIO outputs via 8x L293Q
- USART2 RS485 communication via LTC2864
- Binary packet protocol with CRC-16 CCITT
- Commands: Set/Get/SetAll/GetAll/ClearAll/Enable/Disable/GetEnable
