#pragma once

#include <cstdint>
#include <functional>
#include <condition_variable>
#include <dbus-c++/dbus.h>
#include <glib.h>

struct GPSData
{
    int32_t positionAccuracy = 0;
    uint64_t uTCtime = 0;
    double latitude = 0;
    double longitude = 0;
    int32_t altitude = 0;
    double heading = 0;
    double velocity = 0;
    double horizontalAccuracy = 0;
    double verticalAccuracy = 0;

    bool IsSame(const GPSData& other) const;
};

void mzd_gps2_start(DBus::Connection& serviceBus);

bool mzd_gps2_get(GPSData& data);
void mzd_gps2_set_enabled(bool bEnabled);

void mzd_gps2_stop();

gboolean gps_func(gpointer user_data);

