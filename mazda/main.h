#pragma once

#include <glib.h>

#include "hu_aap.h"

struct gst_app_t {
        GMainLoop *loop;
};

extern gst_app_t gst_app;
extern IHUAnyThreadInterface* g_hu;

enum HU_MODE {
    NONE = 0,
    USB = 1,
    WIFI_AP = 2,
    WIFI_CLIENT = 3
};
