#include <dbus-c++/dbus.h>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <ctime>

#include <glib.h>

#include "../dbus/generated_cmu.h"

#include "hu_uti.h"
#include "hu_aap.h"

#include "config.h"


#include "mzd_gps.h"
#include "../main.h"

// Check the content folder. sd_nav still exists without the card installed
#define SD_CARD_PATH "/tmp/mnt/sd_nav/content"

enum LDSControl
{
    LDS_READ_START = 0,
    LDS_READ_STOP = 1
};

class GPSLDSCLient : public com::jci::lds::data_proxy,
        public DBus::ObjectProxy
{
public:
    explicit GPSLDSCLient(DBus::Connection &connection)
        : DBus::ObjectProxy(connection, "/com/jci/lds/data", "com.jci.lds.data")
    {
    }

    void GPSDiagnostics(const uint8_t& dTCId, const uint8_t& dTCAction) override {}
    void OneTimeDRDiagnostics(const std::string& dRUnitVersion, const int32_t& antennaStatus, const bool& gyroSelfTest, const bool& accelSelfTest, const bool& resetLearning, const bool& saveLearning) override {}
    void PeriodicDRDiagnostics(const int32_t& dRUnitStatus, const int32_t& speedPulse, const bool& reverse, const int32_t& dRUnitMode, const int32_t& gyroStatus, const int32_t& accelStatus) override {}

};

class GPSLDSControl : public com::jci::lds::control_proxy,
        public DBus::ObjectProxy
{
public:
    explicit GPSLDSControl(DBus::Connection &connection)
        : DBus::ObjectProxy(connection, "/com/jci/lds/control", "com.jci.lds.control")
    {
    }

    void ReadStatus(const int32_t& commandReply, const int32_t& status) override;
};

static std::unique_ptr<GPSLDSCLient> gps_client;
static std::unique_ptr<GPSLDSControl> gps_control;
static int get_data_errors_in_a_row = 0;

void GPSLDSControl::ReadStatus(const int32_t& commandReply, const int32_t& status)
{
    //not sure what this does yet
    logw("Read status changed commandReply %i status %i\n", commandReply, status);
}


void mzd_gps2_start(DBus::Connection& serviceBus)
{
    if (gps_client != nullptr)
        return;

    try
    {
        gps_client.reset(new GPSLDSCLient(serviceBus));
        gps_control.reset(new GPSLDSControl(serviceBus));
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: Failed to connect to SERVICE bus %s: %s", error.name(), error.message());
        gps_client.reset();
        gps_control.reset();
        return;
    }

    printf("GPS service connection established.\n");
}

bool mzd_gps2_get(GPSData& data)
{
    if (gps_client == nullptr)
        return false;

    try
    {
        gps_client->GetPosition(data.positionAccuracy, data.uTCtime, data.latitude, data.longitude, data.altitude, data.heading, data.velocity, data.horizontalAccuracy, data.verticalAccuracy);

        if (get_data_errors_in_a_row > 0)
        {
            loge("DBUS: GetPosition hid %i failures", get_data_errors_in_a_row);
            get_data_errors_in_a_row = 0;
        }

        //timestamp 0 means "invalid" and positionAccuracy 0 means "no lock"
        if (data.uTCtime == 0 || data.positionAccuracy == 0)
            return false;

        mzd_gps2_set_enabled(true);
        return true;
    }
    catch(DBus::Error& error)
    {
        get_data_errors_in_a_row++;
        //prevent insane log spam
        if (get_data_errors_in_a_row < 10)
        {
            loge("DBUS: GetPosition failed %s: %s", error.name(), error.message());
        }
        return false;
    }
}

void mzd_gps2_set_enabled(bool bEnabled)
{
    if (gps_control)
    {
        try
        {
            gps_control->ReadControl(bEnabled ? LDS_READ_START : LDS_READ_STOP);
        }
        catch(DBus::Error& error)
        {
            loge("DBUS: ReadControl failed %s: %s", error.name(), error.message());
        }
    }
}

void mzd_gps2_stop()
{
    mzd_gps2_set_enabled(false);
    gps_client.reset();
    gps_control.reset();
}

bool GPSData::IsSame(const GPSData& other) const
{
    if (uTCtime == 0 && other.uTCtime == 0)
        return true; //other members don't matter since timestamp 0 means "invalid"
    return positionAccuracy == other.positionAccuracy &&
            uTCtime == other.uTCtime &&
            int32_t(latitude * 1E7) == int32_t(other.latitude * 1E7) &&
            int32_t(longitude * 1E7) == int32_t(other.longitude * 1E7) &&
            altitude == other.altitude &&
            int32_t(heading * 1E7) == int32_t(other.heading * 1E7) &&
            int32_t(velocity * 1E7) == int32_t(other.velocity * 1E7) &&
            int32_t(horizontalAccuracy * 1E7) == int32_t(other.horizontalAccuracy * 1E7) &&
            int32_t(verticalAccuracy * 1E7) == int32_t(other.verticalAccuracy * 1E7);
}

GPSData data, newData;
time_t oldTs = 0;
int debugLogCount = 0;

gboolean gps_func(gpointer __attribute__((unused)) user_data){
    logd("Getting GPS Data");
    if (config::carGPS && mzd_gps2_get(newData) && !data.IsSame(newData))
    {
        data = newData;
        timeval tv{};
        gettimeofday(&tv, nullptr);
        time_t timestamp = tv.tv_sec * 1000000 + tv.tv_usec;
        if (debugLogCount < 50) //only print the first 50 to avoid spamming the log and breaking the opera text box
        {
            logd("GPS data: %d %d %f %f %d %f %f %f %f   \n",data.positionAccuracy, data.uTCtime, data.latitude, data.longitude, data.altitude, data.heading, data.velocity, data.horizontalAccuracy, data.verticalAccuracy);
            logd("Delta %f\n", (timestamp - oldTs)/1000000.0);
            debugLogCount++;
        }
        oldTs = timestamp;

        g_hu->hu_queue_command([data, timestamp](IHUConnectionThreadInterface& s)
                               {
                                   HU::SensorEvent sensorEvent;
                                   HU::SensorEvent::LocationData* location = sensorEvent.add_location_data();
                                   //AA uses uS and the gps data just has seconds, just use the current time to get more precision so AA can
                                   //interpolate better
                                   location->set_timestamp(static_cast<unsigned long long int>(timestamp));
                                   location->set_latitude(static_cast<int32_t>(data.latitude * 1E7));
                                   location->set_longitude(static_cast<int32_t>(data.longitude * 1E7));

                                   // If the sd card exists then reverse heading. This should only be used on installs that have the
                                   // reversed heading issue.
                                   double newHeading = data.heading;

                                   if (config::reverseGPS)
                                   {
                                       const char* sdCardFolder;
                                       sdCardFolder = SD_CARD_PATH;
                                       struct stat sb{};

                                       if (stat(sdCardFolder, &sb) == 0 && S_ISDIR(sb.st_mode))
                                       {
                                           newHeading = data.heading + 180;
                                           if (newHeading >= 360)
                                           {
                                               newHeading = newHeading - 360;
                                           }
                                       }
                                   }

                                   location->set_bearing(static_cast<int32_t>(newHeading * 1E6));
                                   //assuming these are the same units as the Android Location API (the rest are)
                                   double velocityMetersPerSecond = data.velocity * 0.277778; //convert km/h to m/s
                                   location->set_speed(static_cast<int32_t>(velocityMetersPerSecond * 1E3));

                                   location->set_altitude(static_cast<int32_t>(data.altitude * 1E2));
                                   location->set_accuracy(static_cast<unsigned int>(data.horizontalAccuracy * 1E3));

                                   s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
                               });
    }
    logd("Done Getting GPS Data");
    return true;
}
