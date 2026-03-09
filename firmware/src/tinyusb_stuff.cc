#include <tusb.h>
#include <cstring>

#include "config.h"
#include "globals.h"
#include "our_descriptor.h"
#include "platform.h"
#include "remapper.h"

// Forçamos o VID e PID do Logitech G600
#define USB_VID 0x046D
#define USB_PID 0xC24A //go

tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x7702, // Versão de firmware do G600

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

// Mantemos as configurações originais do autor para a interface web funcionar perfeitamente
const uint8_t configuration_descriptor0[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250), // 500mA
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, our_descriptors[0].descriptor_length, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, config_report_descriptor_length, 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor1[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, our_descriptors[1].descriptor_length, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, config_report_descriptor_length, 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor2[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_HID_DESC_LEN, 0, 250),
    TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[2].descriptor_length, 0x02, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, config_report_descriptor_length, 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor3[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, 0, 250),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[3].descriptor_length, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, config_report_descriptor_length, 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor4[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_HID_DESC_LEN, 0, 250),
    TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[4].descriptor_length, 0x02, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, config_report_descriptor_length, 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor5[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, 0, 250),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[5].descriptor_length, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, config_report_descriptor_length, 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t* configuration_descriptors[] = {
    configuration_descriptor0,
    configuration_descriptor1,
    configuration_descriptor2,
    configuration_descriptor3,
    configuration_descriptor4,
    configuration_descriptor5,
};

// Aqui aplicamos a identidade do G600
char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: English
    "Logitech",                 // 1: Manufacturer
    "Gaming Mouse G600",        // 2: Product
    "9E032E3CB4740017",         // 3: Serial Original
};

uint8_t const* tud_descriptor_device_cb() {
    // Blindagem de Identidade: Garante que os valores sempre fiquem travados no G600
    // independentemente do que o usuário salvar na interface web
    desc_device.idVendor = USB_VID;
    desc_device.idProduct = USB_PID;
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return configuration_descriptors[our_descriptor->idx];
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    if (itf == 0) {
        return our_descriptor->descriptor;
    } else if (itf == 1) {
        return config_report_descriptor;
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

void tud_suspend_cb(bool remote_wakeup_en) { }
void tud_resume_cb() { }
