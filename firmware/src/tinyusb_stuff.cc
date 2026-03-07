#include <tusb.h> //

#include "config.h"
#include "globals.h"
#include "our_descriptor.h"
#include "platform.h"
#include "remapper.h"

// Forçando a assinatura estrita do Logitech G600
#define USB_VID 0x046D
#define USB_PID 0xC24A
#define G600_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)

tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    
    // CORREÇÃO CRÍTICA: O Endpoint 0 do RP2350 precisa ser 64 (Evita o Erro 43)
    .bMaxPacketSize0 = 64, 

    .idVendor = 0x046D,
    .idProduct = 0xC24A,
    .bcdDevice = 0x7702,     // Firmware Version G600 original

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,   // Força a buscar String exata do Serial

    .bNumConfigurations = 0x01,
};

const uint8_t configuration_descriptor_g600[] = {
    // Config: 1, Interfaces: 2, String Index: 4, Tamanho Total, Atributos, 500mA
    TUD_CONFIG_DESCRIPTOR(1, 2, 4, G600_CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),
    
    // Interface 0: Mouse (Agora com 50 bytes)
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_MOUSE, 50, 0x81, 9, 1),
    
    // Interface 1: Keyboard/Vendor (Agora com 63 bytes)
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_KEYBOARD, 63, 0x83, 32, 1),

};

char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: English
    "Logitech",                 // 1: Manufacturer
    "Gaming Mouse G600",        // 2: Product
    "9E032E3CB4740017",         // 3: Serial Real do G600
    "U77.02_B0017"              // 4: Configuração do G600
};

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const* tud_descriptor_device_cb() {
    return (uint8_t const*) &desc_device;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return configuration_descriptor_g600; 
}

// Invoked when received GET HID REPORT DESCRIPTOR
uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    if (itf == 0) {
        return g600_mouse_report_descriptor; // 72 bytes do Mouse
    } else if (itf == 1) {
        return g600_intf1_report_descriptor; // 231 bytes do Teclado/Vendor
    }
    return NULL;
}

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
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
        if (chr_count > 31)
            chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    if (itf == 0) {
        return handle_get_report0(report_id, buffer, reqlen);
    } else {
        return handle_get_report1(report_id, buffer, reqlen);
    }
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (itf == 0) {
        if ((report_id == 0) && (report_type == 0) && (bufsize > 0)) {
            report_id = buffer[0];
            buffer++;
        }
        handle_set_report0(report_id, buffer, bufsize);
    } else {
        handle_set_report1(report_id, buffer, bufsize);
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

void tud_suspend_cb(bool remote_wakeup_en) {
}

void tud_resume_cb() {
}
