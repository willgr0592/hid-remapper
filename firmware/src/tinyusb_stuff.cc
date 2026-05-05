#include <tusb.h>
#include <cstring>
#include <cstdio>

#include <pico/unique_id.h>

#include "config.h"
#include "globals.h"
#include "our_descriptor.h"
#include "platform.h"
#include "remapper.h"

// Microsoft Wired Keyboard 600 (ships as keyboard+mouse combo)
#define USB_VID 0x045E
#define USB_PID 0x0750

// Report IDs matching our HID descriptors
const uint8_t REPORT_ID_MOUSE = 1;
const uint8_t REPORT_ID_KEYBOARD = 2;
const uint8_t REPORT_ID_CONSUMER = 3;

tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0002,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

// Interface 0: Mouse
// Report ID 1: 5 buttons (left/right/mid/back/fwd), X/Y rel int16, wheel int8, pan int8
// Total input report: 1 (ID) + 1 (buttons+pad) + 2 (X) + 2 (Y) + 1 (wheel) + 1 (pan) = 8 bytes
static const uint8_t desc_ms_mouse[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x85, 0x01,        //     Report ID (1)

    // 5 buttons
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x05,        //     Usage Maximum (5)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x05,        //     Report Count (5)
    0x81, 0x02,        //     Input (Data, Var, Abs)
    // 3 padding bits
    0x75, 0x03,        //     Report Size (3)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x03,        //     Input (Const, Var, Abs)

    // X, Y relative int16
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x16, 0x00, 0x80,  //     Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data, Var, Rel)

    // Wheel int8
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data, Var, Rel)

    // Horizontal scroll (AC Pan) int8
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data, Var, Rel)

    0xC0,              //   End Collection
    0xC0,              // End Collection
};

// Interface 1: Keyboard — standard 6KRO boot-compatible layout with LED output.
// Report ID 2  (Input):   [modifier][reserved][k0][k1][k2][k3][k4][k5] = 8 bytes
// Report ID 98 (Output):  LED state byte (NumLock/CapsLock/ScrollLock/Compose/Kana)
// Report ID 100 (Feature): web configurator command channel — Feature-only, never
//   polled by the OS or games; only accessed on explicit GET_REPORT from the web UI.
// Report ID 101 (Input/Feature): monitor channel — same story.
// The vendor collections are appended after the keyboard End Collection so the
// Windows HID class driver binds only the keyboard top-level collection (Application)
// and ignores the vendor ones entirely.
static const uint8_t desc_ms_kb[] = {
    0x05, 0x01,              // Usage Page (Generic Desktop)
    0x09, 0x06,              // Usage (Keyboard)
    0xA1, 0x01,              // Collection (Application)
    0x85, 0x02,              //   Report ID (2)

    // Modifier keys (8 bits)
    0x05, 0x07,              //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,              //   Usage Minimum (Left Ctrl)
    0x29, 0xE7,              //   Usage Maximum (Right GUI)
    0x15, 0x00,              //   Logical Minimum (0)
    0x25, 0x01,              //   Logical Maximum (1)
    0x75, 0x01,              //   Report Size (1)
    0x95, 0x08,              //   Report Count (8)
    0x81, 0x02,              //   Input (Data, Var, Abs)

    // Reserved byte
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x01,              //   Report Count (1)
    0x81, 0x03,              //   Input (Const, Var, Abs)

    // 6 key slots (6KRO array)
    0x05, 0x07,              //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,              //   Usage Minimum (0)
    0x29, 0x65,              //   Usage Maximum (101)
    0x15, 0x00,              //   Logical Minimum (0)
    0x25, 0x65,              //   Logical Maximum (101)
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x06,              //   Report Count (6)
    0x81, 0x00,              //   Input (Data, Array, Abs)

    // LED output (NumLock, CapsLock, ScrollLock, Compose, Kana + 3 pad bits)
    0x85, REPORT_ID_LEDS,    //   Report ID (98)
    0x05, 0x08,              //   Usage Page (LEDs)
    0x19, 0x01,              //   Usage Minimum (Num Lock)
    0x29, 0x05,              //   Usage Maximum (Kana)
    0x25, 0x01,              //   Logical Maximum (1)
    0x75, 0x01,              //   Report Size (1)
    0x95, 0x05,              //   Report Count (5)
    0x91, 0x02,              //   Output (Data, Var, Abs)
    0x75, 0x03,              //   Report Size (3)
    0x95, 0x01,              //   Report Count (1)
    0x91, 0x03,              //   Output (Const, Var, Abs)

    0xC0,                    // End Collection (Keyboard)

    // Web configurator command channel — Feature-only, never touched by HID class driver
    0x06, 0x00, 0xFF,        // Usage Page (Vendor 0xFF00)
    0x09, 0x20,              // Usage (0x20)
    0xA1, 0x01,              // Collection (Application)
    0x85, REPORT_ID_CONFIG,  //   Report ID (100)
    0x09, 0x20,              //   Usage (0x20)
    0x15, 0x00,              //   Logical Minimum (0)
    0x26, 0xFF, 0x00,        //   Logical Maximum (255)
    0x75, 0x08,              //   Report Size (8)
    0x95, CONFIG_SIZE,       //   Report Count (32)
    0xB1, 0x02,              //   Feature (Data, Var, Abs)
    0x85, REPORT_ID_MONITOR, //   Report ID (101)
    0x09, 0x21,              //   Usage (0x21)
    0x95, 0x3F,              //   Report Count (63)
    0x81, 0x02,              //   Input (Data, Var, Abs)
    0xC0,                    // End Collection (Vendor)
};

static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0,
        TUD_CONFIG_DESC_LEN + (TUD_HID_DESC_LEN * 2),
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 50),

    // Interface 0: Mouse (EP 0x81, 8-byte reports, 1ms polling)
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_MOUSE,
        sizeof(desc_ms_mouse), 0x81, 8, 1),

    // Interface 1: Keyboard (EP 0x82, 64-byte EP to support monitor input reports up to 63 bytes, 1ms polling)
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(desc_ms_kb), 0x82, 64, 1),
};

static char serial_str[13];  // 12 hex chars + null

static void init_serial() {
    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    // Take the lower 48 bits (6 bytes) — yields a 12-char hex serial
    snprintf(serial_str, sizeof(serial_str), "%02X%02X%02X%02X%02X%02X",
             uid.id[2], uid.id[3], uid.id[4], uid.id[5], uid.id[6], uid.id[7]);
}

char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04},  // 0: English (0x0409)
    "Microsoft",                  // 1: Manufacturer
    "Wired Keyboard 600",         // 2: Product
    serial_str,                   // 3: Serial (derived from board unique ID)
};

uint8_t const* tud_descriptor_device_cb() {
    static bool serial_ready = false;
    if (!serial_ready) {
        init_serial();
        serial_ready = true;
    }
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return configuration_descriptor;
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    if (itf == 0) return desc_ms_mouse;
    if (itf == 1) return desc_ms_kb;
    return NULL;
}

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
            return NULL;

        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    if (itf == 1) {
        return handle_get_report1(report_id, buffer, reqlen);
    } else {
        return handle_get_report0(report_id, buffer, reqlen);
    }
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (itf == 1) {
        handle_set_report1(report_id, buffer, bufsize);
    } else {
        if ((report_id == 0) && (report_type == 0) && (bufsize > 0)) {
            report_id = buffer[0];
            buffer++;
        }
        handle_set_report0(report_id, buffer, bufsize);
    }
}

void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    boot_protocol_keyboard = (protocol == HID_PROTOCOL_BOOT);
    boot_protocol_updated = true;
}

void tud_mount_cb() {
    reset_resolution_multiplier();
    if (boot_protocol_keyboard) {
        boot_protocol_keyboard = false;
        boot_protocol_updated = true;
    }
}

void tud_suspend_cb(bool remote_wakeup_en) { }
void tud_resume_cb() { }
