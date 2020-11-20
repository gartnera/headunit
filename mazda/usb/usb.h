#pragma once

#include <dbus-c++/dbus.h>
#include <libudev.h>
#include <functional>
#include <condition_variable>

#define VEN_ID_GOOGLE           0x18D1
#define DEV_ID_OAP              0x2D00
#define DEV_ID_OAP_WITH_ADB     0x2D01

#define AOA_GET_PROTOCOL		51

static int check_aoa(struct udev_device *device);
void udev_thread_func(std::promise<int> * promObj, std::atomic<bool> *detection_done);

