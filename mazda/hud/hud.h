#ifndef MZD_HUD_H
#define MZD_HUD_H

#include <stdint.h>
#include <string>
#include <functional>
#include <condition_variable>
#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>

#include "../dbus/generated_cmu.h"

enum HudDistanceUnit: uint8_t {
    METERS = 1,
    MILES = 2,
    KILOMETERS = 3,
    YARDS = 4,
    FEET = 5
};

struct NaviData {
  std::string event_name;
  int32_t turn_side;
  int32_t turn_event;
  int32_t turn_number;
  int32_t turn_angle;
  int32_t distance; // distance * 10, encoded like that to store one digit after decimal dot in int type
  HudDistanceUnit distance_unit; 
  int32_t time_until;
  uint8_t previous_msg;
  uint8_t changed;
};

enum NaviTurns: uint32_t {
  STRAIGHT = 1,
  LEFT = 2,
  RIGHT = 3,
  SLIGHT_LEFT = 4,
  SLIGHT_RIGHT = 5,
  DESTINATION  = 8,
  DESTINATION_LEFT = 33,
  DESTINATION_RIGHT = 34,
  SHARP_LEFT = 11,
  SHARP_RIGHT = 9,
  U_TURN_LEFT = 13,
  U_TURN_RIGHT = 10,
  FLAG = 12,
  FLAG_LEFT = 35,
  FLAG_RIGHT = 36,
  FORK_LEFT = 15,
  FORK_RIGHT = 14,
  MERGE_LEFT = 16,
  MERGE_RIGHT = 17,
  OFF_RAMP_LEFT = 7,
  OFF_RAMP_RIGHT = 30
};

void hud_start();
void hud_stop();
bool hud_installed();
void hud_thread_func(std::condition_variable& quitcv, std::mutex& quitmutex, std::mutex& hudmutex);

class HUDSettingsClient : public com::jci::navi2IHU::HUDSettings_proxy,
                     public DBus::ObjectProxy
{
public:
    HUDSettingsClient(DBus::Connection &connection, const char *path, const char *name)
        : DBus::ObjectProxy(connection, path, name)
    {
    }

    virtual void HUDInstalledChanged(const bool& hUDInstalled) override {}
    virtual void SetHUDSettingFailed(const int32_t& hUDSettingType, const int32_t& err) override {}
    virtual void HUDControlAllowed(const bool& bAllowed) override {}
    virtual void HUDSettingChanged(const int32_t& hUDSettingType, const int32_t& value) override {}
};

class NaviClient : public com::jci::vbs::navi_proxy,
                     public DBus::ObjectProxy
{
public:
    NaviClient(DBus::Connection &connection, const char *path, const char *name)
        : DBus::ObjectProxy(connection, path, name)
    {
    }

    virtual void FuelTypeResp(const uint8_t& fuelType) override {}
    virtual void HUDResp(const uint8_t& hudStatus) override {}
    virtual void TSRResp(const uint8_t& tsrStatus) override {}
    virtual void GccConfigMgmtResp(const ::DBus::Struct< std::vector< uint8_t > >& vin_Character) override {}
    virtual void TSRFeatureMode(const uint8_t& tsrMode) override {}
};

  class TMCClient : public com::jci::vbs::navi::tmc_proxy,
                       public DBus::ObjectProxy
  {
  public:
      TMCClient(DBus::Connection &connection, const char *path, const char *name)
          : DBus::ObjectProxy(connection, path, name)
      {
      }

      virtual void ServiceListResponse(const ::DBus::Struct< uint8_t, std::vector< uint8_t >, std::vector< uint8_t >, std::vector< uint8_t >, std::vector< uint8_t >, std::vector< uint8_t > >& providerList) override {}
      virtual void ResponseToTMCSelection(const uint8_t& rdstmcOperation, const uint8_t& tmcSearchMode, const uint8_t& countryCode, const uint8_t& locationTableNumber, const uint8_t& serviceIdentifier, const uint8_t& quality, const uint8_t& receptionStatus) override {}
};


#endif
