#pragma once

#include <glib.h>

#include "hu_aap.h"

struct gst_app_t {
        GMainLoop *loop;
};

extern gst_app_t gst_app;
extern IHUAnyThreadInterface* g_hu;
