#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "plugin_common.h"

typedef struct libusb_device libusb_device;

struct libusb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
};

attr_public const char *g_pluginName = "usb_probe";
attr_public const char *g_pluginDesc = "Read-only USB descriptor probe";
attr_public const char *g_pluginAuth = "OpenAI diagnostic build";
attr_public uint32_t g_pluginVersion = 0x00000100;

int32_t attr_module_hidden module_start(size_t argc, const void *args) {
    (void)argc;
    (void)args;

    FILE *f = fopen("/data/GoldHEN/usb_probe.txt", "w");
    if (!f) {
        return 0;
    }

    fprintf(f, "USB probe-only diagnostic\n");
    fprintf(f, "This build only enumerates descriptors; it does not claim/open devices and does not hook controller input.\n");

    char module[256] = {0};
    snprintf(module, sizeof(module), "/%s/common/lib/%s",
             sceKernelGetFsSandboxRandomWord(), "libSceUsbd.sprx");

    int h = 0;
    int load_ret = sys_dynlib_load_prx(module, &h);
    fprintf(f, "load libSceUsbd ret=%d handle=%d\n", load_ret, h);
    if (load_ret != 0) {
        fclose(f);
        return 0;
    }

    int (*sceUsbdInit)(void) = NULL;
    int (*sceUsbdExit)(void) = NULL;
    ssize_t (*sceUsbdGetDeviceList)(libusb_device*** list) = NULL;
    int (*sceUsbdGetDeviceDescriptor)(libusb_device* device, struct libusb_device_descriptor* desc) = NULL;
    void (*sceUsbdFreeDeviceList)(libusb_device** list, int unrefDevices) = NULL;

    int r1 = sys_dynlib_dlsym(h, "sceUsbdInit", &sceUsbdInit);
    int r2 = sys_dynlib_dlsym(h, "sceUsbdExit", &sceUsbdExit);
    int r3 = sys_dynlib_dlsym(h, "sceUsbdGetDeviceList", &sceUsbdGetDeviceList);
    int r4 = sys_dynlib_dlsym(h, "sceUsbdGetDeviceDescriptor", &sceUsbdGetDeviceDescriptor);
    int r5 = sys_dynlib_dlsym(h, "sceUsbdFreeDeviceList", &sceUsbdFreeDeviceList);

    fprintf(f, "dlsym: init=%d exit=%d list=%d desc=%d free=%d\n", r1, r2, r3, r4, r5);

    if (!sceUsbdInit || !sceUsbdExit || !sceUsbdGetDeviceList ||
        !sceUsbdGetDeviceDescriptor || !sceUsbdFreeDeviceList) {
        fprintf(f, "RESULT: required USB symbols missing\n");
        fclose(f);
        return 0;
    }

    int init_ret = sceUsbdInit();
    fprintf(f, "sceUsbdInit=%d\n", init_ret);
    if (init_ret != 0) {
        fprintf(f, "RESULT: sceUsbdInit failed\n");
        fclose(f);
        return 0;
    }

    libusb_device **list = NULL;
    ssize_t count = sceUsbdGetDeviceList(&list);
    fprintf(f, "device_count=%lld\n", (long long)count);

    if (count > 0 && list) {
        for (ssize_t i = 0; i < count; ++i) {
            struct libusb_device_descriptor d;
            memset(&d, 0, sizeof(d));
            int dr = sceUsbdGetDeviceDescriptor(list[i], &d);
            fprintf(f,
                    "USB[%lld] ret=%d VID:PID=%04x:%04x bcdUSB=%04x class=%02x sub=%02x proto=%02x configs=%u\n",
                    (long long)i, dr, d.idVendor, d.idProduct, d.bcdUSB,
                    d.bDeviceClass, d.bDeviceSubClass, d.bDeviceProtocol,
                    d.bNumConfigurations);
        }
    }

    if (list) {
        sceUsbdFreeDeviceList(list, 1);
    }

    int exit_ret = sceUsbdExit();
    fprintf(f, "sceUsbdExit=%d\n", exit_ret);
    fprintf(f, "RESULT: probe complete\n");
    fclose(f);

    return 0;
}

int32_t attr_module_hidden module_stop(size_t argc, const void *args) {
    (void)argc;
    (void)args;
    return 0;
}
