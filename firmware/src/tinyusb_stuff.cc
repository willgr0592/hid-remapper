#include <tusb.h>
#include <cstring>

#include "config.h"
#include "globals.h"
#include "our_descriptor.h"
#include "platform.h"
#include "remapper.h"

// Forçamos o VID e PID do Logitech G600
#define USB_VID 0x046D
#define USB_PID 0xC24A

tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 32, // Clonagem 100% exata da assinatura do G600

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x7706, // Mudamos para 06 para forçar o Windows a limpar o cache novamente!

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

// ---------------------------------------------------------------------
// Descriptores separados e isolados para garantir o spoofing 100% G600
// ---------------------------------------------------------------------
const uint8_t desc_g600_mouse[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01, 0x65, 0x00, 0x55, 0x00, 0x75, 0x01,
    0x95, 0x10, 0x81, 0x02, 0x05, 0x01, 0x09, 0x30, 0x26, 0xFF, 0x7F, 0x45, 0x00, 0x75, 0x10, 0x95,
    0x01, 0x81, 0x06, 0x09, 0x31, 0x81, 0x06, 0x09, 0x38, 0x25, 0x7F, 0x75, 0x08, 0x81, 0x06, 0x05,
    0x0C, 0x0A, 0x38, 0x02, 0x81, 0x06, 0xC0, 0xC0
};

// Teclado + Vendor G600 + Web Configuração do HID-Remapper
const uint8_t desc_g600_itf1[] = {
    // -- KEYBOARD --
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x02, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x35, 0x00, 0x45, 0x01, 0x65, 0x00, 0x55, 0x00, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x28, 0x81, 0x03, 0xC0,
    
    // -- VENDOR LOGITECH --
    0x06, 0x80, 0xFF, 0x09, 0x80, 0xA1, 0x01, 0x85, 0x80, 0x09, 0x80, 0x15, 0x00, 0x25, 0xA4, 0x35,
    0x00, 0x45, 0x00, 0x65, 0x00, 0x55, 0x00, 0x75, 0x08, 0x95, 0x05, 0x81, 0x02, 0x25, 0x01, 0x45,
    0x01, 0x75, 0x01, 0x95, 0xD0, 0x81, 0x03, 0x85, 0xF6, 0x09, 0xF6, 0x25, 0xA4, 0x45, 0x00, 0x75,
    0x08, 0x95, 0x07, 0x81, 0x02, 0x25, 0x01, 0x45, 0x01, 0x75, 0x01, 0x95, 0xC0, 0x81, 0x03, 0x85,
    0xF7, 0x09, 0xF7, 0x25, 0xA4, 0x45, 0x00, 0x75, 0x08, 0x95, 0x1F, 0x81, 0x02, 0x85, 0xF0, 0x09,
    0xF0, 0x95, 0x03, 0xB1, 0x02, 0x25, 0x01, 0x45, 0x01, 0x75, 0x01, 0x96, 0xB0, 0x04, 0xB1, 0x03,
    0x85, 0xF1, 0x09, 0xF1, 0x25, 0xA4, 0x45, 0x00, 0x75, 0x08, 0x95, 0x07, 0xB1, 0x02, 0x25, 0x01,
    0x45, 0x01, 0x75, 0x01, 0x96, 0x90, 0x04, 0xB1, 0x03, 0x85, 0xF2, 0x09, 0xF2, 0x25, 0xA4, 0x45,
    0x00, 0x75, 0x08, 0x95, 0x04, 0xB1, 0x02, 0x25, 0x01, 0x45, 0x01, 0x75, 0x01, 0x96, 0xA8, 0x04,
    0xB1, 0x03, 0x85, 0xF3, 0x09, 0xF3, 0x25, 0xA4, 0x45, 0x00, 0x75, 0x08, 0x95, 0x99, 0xB1, 0x02,
    0x85, 0xF4, 0x09, 0xF4, 0xB1, 0x02, 0x85, 0xF5, 0x09, 0xF5, 0xB1, 0x02, 0x85, 0xF6, 0x09, 0xF6,
    0x95, 0x07, 0xB1, 0x02, 0x25, 0x01, 0x45, 0x01, 0x75, 0x01, 0x96, 0x90, 0x04, 0xB1, 0x03, 0xC0,

    // -- REMAPPER WEB CONFIG (Invisível para jogos, Report IDs 100 e 101) --
    0x06, 0x00, 0xFF, 0x09, 0x20, 0xA1, 0x01, 0x09, 0x20, 0x85, 100, 0x75, 0x08, 0x95, 32, 0xB1, 
    0x02, 0xC0, 0x09, 0x21, 0xA1, 0x01, 0x09, 0x21, 0x85, 101, 0x75, 0x08, 0x95, 63, 0x81, 0x02, 0xC0
};

const uint8_t configuration_descriptor_g600[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 2, 4, TUD_CONFIG_DESC_LEN + (25 * 2), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500), 
    
    // Interface 0: Mouse
    9, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_HID, 0x01, 0x02, 0x00,
    9, HID_DESC_TYPE_HID, 0x11, 0x01, 0x00, 1, HID_DESC_TYPE_REPORT, (uint8_t)(sizeof(desc_g600_mouse) & 0xFF), (uint8_t)(sizeof(desc_g600_mouse) >> 8),
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, 0x09, 0x00, 1, // Otimização de silício: 9 bytes

    // Interface 1: Teclado + Vendor + Web Config
    9, TUSB_DESC_INTERFACE, 1, 0, 1, TUSB_CLASS_HID, 0x00, 0x01, 0x00,
    9, HID_DESC_TYPE_HID, 0x11, 0x01, 0x00, 1, HID_DESC_TYPE_REPORT, (uint8_t)(sizeof(desc_g600_itf1) & 0xFF), (uint8_t)(sizeof(desc_g600_itf1) >> 8),
    7, TUSB_DESC_ENDPOINT, 0x82, TUSB_XFER_INTERRUPT, 0x20, 0x00, 1  // Otimização de silício: 32 bytes
};

// Aqui aplicamos a identidade do G600
char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: English
    "Logitech",                 // 1: Manufacturer
    "Gaming Mouse G600",        // 2: Product
    "9E032E3CB4740017",         // 3: Serial Original
    "U77.02_B0017",             // 4: Configuration String
};

uint8_t const* tud_descriptor_device_cb() {
    // Blindagem de Identidade: Garante que os valores sempre fiquem travados no G600
    // independentemente do que o usuário salvar na interface web
    desc_device.idVendor = USB_VID;
    desc_device.idProduct = USB_PID;
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return configuration_descriptor_g600; // Desvincula da EEPROM e força a usar nossa topology
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    if (itf == 0) {
        return desc_g600_mouse;
    } else if (itf == 1) {
        return desc_g600_itf1;
    }
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
    // Roteamento inteligente baseado no ID
    if (report_id == REPORT_ID_CONFIG || report_id == REPORT_ID_MONITOR) {
        return handle_get_report1(report_id, buffer, reqlen);
    } else {
        return handle_get_report0(report_id, buffer, reqlen);
    }
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (report_id == REPORT_ID_CONFIG || report_id == REPORT_ID_MONITOR) {
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
