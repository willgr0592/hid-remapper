#include <cstdio>
#include <cstring>

#include <tusb.h>
#include "hardware/uart.h"

#include "descriptor_parser.h"
#include "remapper.h"
#include "uart_cmd.h"

// We use uart0 which is already initialized by stdio_init_all() on GPIO 0 (TX) / GPIO 1 (RX).
// Baud rate is set by PICO_DEFAULT_UART_BAUD_RATE (921600 from CMakeLists.txt).
// The CP2102 module on the other end must match this baud rate.
#define UART_CMD_INST uart0

// Fake device identity for the UART command source.
// This registers as a separate input device in the remapper so UART
// commands get combined with real mouse input via unmapped passthrough.
#define UART_FAKE_VID       0x0002
#define UART_FAKE_PID       0x0002
#define UART_FAKE_INTERFACE 0x0201

// HID report descriptor for the fake UART input device.
// No Report ID. Matches G600 mouse usages so unmapped passthrough works.
// Report layout (8 bytes):
//   Bytes 0-1: Buttons 1-16 (16 x 1-bit, absolute)
//   Bytes 2-3: X (int16_t, relative)
//   Bytes 4-5: Y (int16_t, relative)
//   Byte  6:   Wheel (int8_t, relative)
//   Byte  7:   AC Pan (int8_t, relative)
static const uint8_t uart_fake_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)

    // 16 buttons
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x10,        //     Usage Maximum (16)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x10,        //     Report Count (16)
    0x81, 0x02,        //     Input (Data,Var,Abs)

    // X, Y (16-bit signed relative)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x16, 0x00, 0x80,  //     Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data,Var,Rel)

    // Wheel (8-bit signed relative)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x80,        //     Logical Minimum (-128)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data,Var,Rel)

    // AC Pan (8-bit signed relative)
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x15, 0x80,        //     Logical Minimum (-128)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data,Var,Rel)

    0xC0,              //   End Collection
    0xC0,              // End Collection
};

// Report structure matching uart_fake_descriptor (8 bytes, no report ID)
struct __attribute__((packed)) uart_mouse_report_t {
    uint16_t buttons;
    int16_t dx;
    int16_t dy;
    int8_t wheel;
    int8_t pan;
};

// Persistent button state (buttons are absolute, so we track state across commands)
static uint16_t button_state = 0;

// Persistent keyboard state — kept across commands so keys stay held
static uint8_t kb_modifier_state = 0;
static uint8_t kb_keycode_state = 0;

static void send_kb_report() {
    uint8_t kb_report[8] = {0};
    kb_report[0] = kb_modifier_state;
    // kb_report[1] = 0 (reserved)
    kb_report[2] = kb_keycode_state;
    tud_hid_n_report(1, 2, kb_report, sizeof(kb_report));
}

// Protocol parser state machine
enum ParseState : uint8_t {
    WAIT_SYNC,
    READ_CMD,
    READ_DATA,
    READ_CHECKSUM,
};

static ParseState parse_state = WAIT_SYNC;
static uint8_t cmd = 0;
static uint8_t data_buf[8];
static uint8_t data_idx = 0;
static uint8_t data_len = 0;

static uint8_t cmd_data_length(uint8_t c) {
    switch (c) {
        case UART_CMD_MOVE:    return 4;
        case UART_CMD_BUTTONS: return 2;
        case UART_CMD_SCROLL:  return 2;
        case UART_CMD_REPORT:  return 8;
        case UART_CMD_CLICK:   return 2;
        case UART_CMD_KEY:     return 2;
        default:               return 0;
    }
}

static void send_response(uint8_t resp_cmd, uint8_t status) {
    if (uart_is_writable(UART_CMD_INST)) {
        uart_putc_raw(UART_CMD_INST, UART_CMD_RESP);
    }
    if (uart_is_writable(UART_CMD_INST)) {
        uart_putc_raw(UART_CMD_INST, resp_cmd);
    }
    if (uart_is_writable(UART_CMD_INST)) {
        uart_putc_raw(UART_CMD_INST, status);
    }
}

static void inject_report(int16_t dx, int16_t dy, int8_t wheel, int8_t pan) {
    uart_mouse_report_t report;
    report.buttons = button_state;
    report.dx = dx;
    report.dy = dy;
    report.wheel = wheel;
    report.pan = pan;

    handle_received_report((const uint8_t*) &report, sizeof(report), UART_FAKE_INTERFACE);
}

static bool execute_cmd() {
    switch (cmd) {
        case UART_CMD_MOVE: {
            int16_t dx = (int16_t) (data_buf[0] | (data_buf[1] << 8));
            int16_t dy = (int16_t) (data_buf[2] | (data_buf[3] << 8));
            inject_report(dx, dy, 0, 0);
            return true;
        }
        case UART_CMD_BUTTONS: {
            button_state = data_buf[0] | (data_buf[1] << 8);
            inject_report(0, 0, 0, 0);
            return true;
        }
        case UART_CMD_SCROLL: {
            int8_t wheel = (int8_t) data_buf[0];
            int8_t pan = (int8_t) data_buf[1];
            inject_report(0, 0, wheel, pan);
            return true;
        }
        case UART_CMD_REPORT: {
            button_state = data_buf[0] | (data_buf[1] << 8);
            int16_t dx = (int16_t) (data_buf[2] | (data_buf[3] << 8));
            int16_t dy = (int16_t) (data_buf[4] | (data_buf[5] << 8));
            int8_t wheel = (int8_t) data_buf[6];
            int8_t pan = (int8_t) data_buf[7];
            inject_report(dx, dy, wheel, pan);
            return true;
        }
        case UART_CMD_CLICK: {
            uint8_t btn = data_buf[0];
            uint8_t action = data_buf[1];
            if (btn < 16) {
                if (action) {
                    button_state |= (1 << btn);
                } else {
                    button_state &= ~(1 << btn);
                }
                inject_report(0, 0, 0, 0);
                return true;
            }
            return false;
        }
        case UART_CMD_KEY: {
            // Update persistent keyboard state and send report on interface 1.
            // State is re-sent every uart_cmd_process() call to keep keys held.
            kb_modifier_state = data_buf[0];
            kb_keycode_state = data_buf[1];
            send_kb_report();
            return true;
        }
        default:
            return false;
    }
}

void uart_cmd_init() {
    // Register the fake device descriptor so the remapper knows about our usages.
    // With unmapped_passthrough enabled (default), these usages will automatically
    // map to matching output usages in the G600 descriptor.
    parse_descriptor(UART_FAKE_VID, UART_FAKE_PID,
                     uart_fake_descriptor, sizeof(uart_fake_descriptor),
                     UART_FAKE_INTERFACE, 0);
}

bool uart_cmd_process() {
    bool injected = false;

    while (uart_is_readable(UART_CMD_INST)) {
        uint8_t c = uart_getc(UART_CMD_INST);

        switch (parse_state) {
            case WAIT_SYNC:
                if (c == UART_CMD_SYNC) {
                    parse_state = READ_CMD;
                }
                break;

            case READ_CMD:
                cmd = c;
                data_len = cmd_data_length(cmd);
                if (data_len == 0) {
                    // Unknown command
                    send_response(cmd, 0x00);
                    parse_state = WAIT_SYNC;
                } else {
                    data_idx = 0;
                    parse_state = READ_DATA;
                }
                break;

            case READ_DATA:
                data_buf[data_idx++] = c;
                if (data_idx >= data_len) {
                    parse_state = READ_CHECKSUM;
                }
                break;

            case READ_CHECKSUM: {
                // Verify XOR checksum over cmd + data bytes
                uint8_t checksum = cmd;
                for (uint8_t i = 0; i < data_len; i++) {
                    checksum ^= data_buf[i];
                }
                if (checksum == c) {
                    bool ok = execute_cmd();
                    send_response(cmd, ok ? 0x01 : 0x00);
                    if (ok) {
                        injected = true;
                    }
                } else {
                    send_response(cmd, 0x00);
                }
                parse_state = WAIT_SYNC;
                break;
            }
        }
    }

    // Re-send keyboard state every loop iteration to keep keys held.
    // This ensures the key stays pressed even if the endpoint was busy
    // or another report on interface 1 was sent in between.
    if (kb_modifier_state != 0 || kb_keycode_state != 0) {
        send_kb_report();
    }

    return injected;
}
