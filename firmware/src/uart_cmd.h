#ifndef _UART_CMD_H_
#define _UART_CMD_H_

#include <stdint.h>

// Binary protocol for receiving mouse commands via UART (CP2102 on GPIO 0/1).
//
// UART settings: 921600 baud, 8N1 (matches PICO_DEFAULT_UART_BAUD_RATE)
//
// Packet format:
//   [0xAA] [CMD] [DATA...] [CHECKSUM]
//   CHECKSUM = XOR of all bytes from CMD through end of DATA
//
// Commands:
//   0x01 MOVE     - 4 data bytes: int16_t dx (LE), int16_t dy (LE)
//   0x02 BUTTONS  - 2 data bytes: uint16_t button_mask (LE), lower 5 bits used
//   0x03 SCROLL   - 2 data bytes: int8_t wheel, int8_t pan
//   0x04 REPORT   - 7 data bytes: uint8_t buttons, int16_t dx (LE), int16_t dy (LE), int8_t wheel, int8_t pan
//   0x05 CLICK    - 2 data bytes: uint8_t button_index (0-4), uint8_t action (0=release, 1=press)
//   0x06 KEY      - 2 data bytes: uint8_t modifier_mask, uint8_t keycode (USB HID keycode, 0=release)
//   0x07 KEY_TAP  - 2 data bytes: transient modifier/keycode tap queued alongside held KEY state
//
// Response (sent back on TX after each command):
//   [0x55] [CMD] [STATUS]
//   STATUS: 0x01 = OK, 0x00 = ERROR

#define UART_CMD_SYNC     0xAA
#define UART_CMD_RESP     0x55

#define UART_CMD_MOVE     0x01
#define UART_CMD_BUTTONS  0x02
#define UART_CMD_SCROLL   0x03
#define UART_CMD_REPORT   0x04
#define UART_CMD_CLICK    0x05
#define UART_CMD_KEY      0x06
#define UART_CMD_KEY_TAP  0x07  // queued single-shot tap; preserves the persistent KEY hold

// Register fake device descriptor for UART input injection.
// Call this from extra_init().
void uart_cmd_init();

// Poll UART for incoming commands and inject reports.
// Call this from the main loop (read_report or similar).
// Returns true if a report was injected.
bool uart_cmd_process();

#endif
