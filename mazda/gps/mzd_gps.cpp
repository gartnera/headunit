#include "mzd_gps.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <inttypes.h>
#include <ctime>

#define LOGTAG "mzd-gps"
#include "../hu/hu_uti.h"

#include "nmeaparse/NMEAParser.h"
#include "nmeaparse/GPSService.h"

static volatile bool running = false;

void* process_gps(void* arg) {
    auto callbackPtr = (void(*)(uint64_t, double, double, double, double, double, double))arg;

    printf("GPS thread started...");

    FILE* fp;
    char* gps_line = NULL;
    size_t len = 0;
    ssize_t read;

    // GPS hardware is attached to ttymxc2 on CMU and communicates
    // via text NMEA protocol.
    fp = fopen("/dev/ttymxc2", "r");
    if (fp == NULL) return NULL;

    // We need to parse NMEA sentences into a solid fix.
    nmea::NMEAParser parser;
    nmea::GPSService gps(parser);

    uint64_t last_timestamp = 0;

    // GPS update callback
    gps.onUpdate += [&gps, callbackPtr, &last_timestamp]() {
        // First check if we have a positive fix, don't report broken fixes.
        auto& fix = gps.fix;
        if (!fix.locked() || fix.horizontalAccuracy() > 80) return;
        if (fix.latitude == 0 && fix.longitude == 0) return;    // Sometimes we get zero here, ignore it then.

        // Don't flood the sensors channel
        if (static_cast<uint64_t>(fix.timestamp.getTime()) - last_timestamp < 500) return;
        last_timestamp = static_cast<uint64_t>(fix.timestamp.getTime());

        // epoch timestamp, latitude, longitude, bearing, speed, altitude, accuracy
        (*callbackPtr)(fix.timestamp.getTime(),
                    fix.latitude,
                    fix.longitude,
                    fix.travelAngle,
                    fix.speed,
                    fix.altitude,
                    fix.horizontalAccuracy());
    };

    while (running && ((read = getline(&gps_line, &len, fp)) != -1)) {
        try {
            parser.readLine(gps_line);
        } catch (nmea::NMEAParseError& e) {
            loge("GPS parse error: %s.", e.message.c_str());
        }
    }

    fclose(fp);
    return nullptr;
}

static pthread_t gps_thread = 0;

void mzd_gps_start(void(*callbackPtr)(uint64_t, double, double, double, double, double, double)) {
    running = true;

    // Using std::thread and/or passing std::function throws an exception on CMU for
    // some reason. Hence store it as a static.
    pthread_create(&gps_thread, NULL, &process_gps, (void*)callbackPtr);
}

void mzd_gps_stop() {
    running = false;
    pthread_join(gps_thread, NULL);
}
