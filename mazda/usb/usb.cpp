//
// Created by silverchris on 2020-11-12.
//

#include <cstdlib>
#include <libudev.h>
#include <libusb.h>
#include <cinttypes>
#include <future>


#include "usb.h"
#include "../main.h"

#include "hu_uti.h"


static int check_aoa(struct udev_device *device) {
    const char *action, *str;
    uint16_t vid = 0;
    uint16_t pid = 0;

    action = udev_device_get_action(device);
    if (action == nullptr) {
        logd("No action got\n");
        return 0;
    }

    logd("@@ [%s] Action=%s\n", __func__, action);

    str = udev_device_get_sysattr_value(device, "idVendor");
    if (str != nullptr) {
        vid = static_cast<uint16_t>(std::stoi(str, nullptr, 16));
        logd("vid: %u\n", vid);
    }
    str = udev_device_get_sysattr_value(device, "idProduct");
    if (str != nullptr) {
        pid = static_cast<uint16_t>(std::stoi(str, nullptr, 16));
        logd("pid: %u\n", pid);
    }

    if(vid == 0 || pid == 0){
        return 0;
    }

    if (vid == VEN_ID_GOOGLE) {
        if (pid == DEV_ID_OAP || pid == DEV_ID_OAP_WITH_ADB) {
            return 1;
        }
    }

    str = udev_device_get_devnode(device);
    if (str != nullptr) {
        logd("devnode: %s\n", str);
    }

    int ret;
    uint8_t buffer[2];
    ret = libusb_init(nullptr);
    if (ret != 0) {
        loge("libusb init failed: %d\n", ret);
        return 0;
    }


    /* Trying to open it */
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(nullptr, vid, pid);
    if (handle == nullptr) {
        loge("Unable to open device...\n");
        return 0;
    }

    /* Now asking if device supports Android Open Accessory protocol */
    ret = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR,
                                  AOA_GET_PROTOCOL, 0, 0, buffer,
                                  sizeof(buffer), 0);
    if (ret < 0) {
        loge("Error getting protocol...\n");
        ret = 0;
    } else {
        int aoa_version = ((buffer[1] << 8) | buffer[0]);
        logd("Device supports AOA %d.0!\n", aoa_version);
        ret = 1;
    }

    libusb_release_interface(handle, 0);
    libusb_close(handle);
    return ret;
}

void udev_thread_func(std::promise<int> *promObj, std::atomic<bool> *detection_done) {
    struct udev *udev;
    struct udev_monitor *udev_monitor;
    int ret, fdcount;
    fd_set readfds;
    struct udev_device *device;
//	const char *syspath;

    udev = udev_new();
    if (udev == nullptr) {
        loge("Error: udev_new\n");
        return;
    }

    udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (udev_monitor == nullptr) {
        loge("Error: udev_monitor_new_from_netlink\n");
        return;
    }

    /* Add some filter */
    ret = udev_monitor_filter_add_match_subsystem_devtype(udev_monitor,"usb",nullptr); //"usb_device");
    if (ret < 0) {
        loge("Error: filter_add_match_subsystem_devtype\n");
        return;
    }

    ret = udev_monitor_enable_receiving(udev_monitor);
    if (ret < 0) {
        loge("udev_monitor_enable_receiving Failed < 0\n");
        return;
    }
    while (!detection_done->load(std::memory_order_relaxed)) {
        FD_ZERO(&readfds);

        FD_SET(udev_monitor_get_fd(udev_monitor), &readfds);

        struct timeval tv = {0, 250000};
        fdcount = select(udev_monitor_get_fd(udev_monitor) + 1, &readfds, nullptr, nullptr, &tv);

        if (fdcount < 0) {
            if (errno != EINTR) {
                loge("Receive Signal!!\n");
                return;
            }
            continue;
        }

        if (!FD_ISSET(udev_monitor_get_fd(udev_monitor), &readfds)) {
            continue;
        }

        device = udev_monitor_receive_device(udev_monitor);
        if (device == nullptr) {
            loge("@@ receive device is not found(check2)\n");
            continue;
        }
        if (check_aoa(device)) {
            logd("Detected AOA usb device");
            promObj->set_value(HU_MODE::USB);
        }
    }
    udev_unref(udev);

}