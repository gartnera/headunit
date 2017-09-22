#pragma once

#include <glib.h>
#include <functional>

#include "hu_aap.h"

struct gst_app_t {
        GMainLoop *loop;
};

extern gst_app_t gst_app;
extern float g_dpi_scalefactor;
extern IHUAnyThreadInterface* g_hu;

uint64_t get_cur_timestamp();
