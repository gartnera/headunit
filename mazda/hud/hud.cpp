#include "hud.h"

#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>
#include <stdint.h>
#include <string>
#include <functional>
#include <condition_variable>
#include <signal.h>
#include <thread>

#include "../dbus/generated_cmu.h"

#define LOGTAG "mazda-hud"

#include "hu_uti.h"

#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"
#define HMI_BUS_ADDRESS "unix:path=/tmp/dbus_hmi_socket"

std::mutex hudmutex;

static HUDSettingsClient *hud_client = NULL;
static NaviClient *vbsnavi_client = NULL;
static TMCClient *tmc_client = NULL;

NaviData *navi_data = NULL;

uint8_t turns[][3] = {
  {0,0,0}, //TURN_UNKNOWN
  {NaviTurns::FLAG_LEFT,NaviTurns::FLAG_RIGHT,NaviTurns::FLAG}, //TURN_DEPART
  {NaviTurns::STRAIGHT,NaviTurns::STRAIGHT,NaviTurns::STRAIGHT}, //TURN_NAME_CHANGE
  {NaviTurns::SLIGHT_LEFT,NaviTurns::SLIGHT_RIGHT,NaviTurns::STRAIGHT}, //TURN_SLIGHT_TURN
  {NaviTurns::LEFT,NaviTurns::RIGHT,0}, //TURN_TURN
  {NaviTurns::SHARP_LEFT,NaviTurns::SHARP_RIGHT,0}, //TURN_SHARP_TURN
  {NaviTurns::U_TURN_LEFT, NaviTurns::U_TURN_RIGHT,0}, //TURN_U_TURN
  {NaviTurns::LEFT,NaviTurns::RIGHT,NaviTurns::STRAIGHT}, //TURN_ON_RAMP
  {NaviTurns::OFF_RAMP_LEFT,NaviTurns::OFF_RAMP_RIGHT,NaviTurns::STRAIGHT}, //TURN_OFF_RAMP
  {NaviTurns::FORK_LEFT, NaviTurns::FORK_RIGHT, 0}, //TURN_FORK
  {NaviTurns::MERGE_LEFT, NaviTurns::MERGE_RIGHT, 0}, //TURN_MERGE
  {0,0,0},  //TURN_ROUNDABOUT_ENTER
  {0,0,0}, // TURN_ROUNDABOUT_EXIT
  {0,0,0}, //TURN_ROUNDABOUT_ENTER_AND_EXIT (Will have to handle seperatly)
  {NaviTurns::STRAIGHT,NaviTurns::STRAIGHT,NaviTurns::STRAIGHT}, //TURN_STRAIGHT
  {0,0,0}, //unused?
  {0,0,0}, //TURN_FERRY_BOAT
  {0,0,0}, //TURN_FERRY_TRAIN
  {0,0,0}, //unused??
  {NaviTurns::DESTINATION_LEFT, NaviTurns::DESTINATION_RIGHT, NaviTurns::DESTINATION} //TURN_DESTINATION
};

uint8_t roundabout(int32_t degrees, int32_t side){
  uint8_t nearest = (degrees + 15) / 30;
  uint8_t offset = side == 0 ? 49 : 37;
  return(nearest + offset);
}

void hud_thread_func(std::condition_variable& quitcv, std::mutex& quitmutex, std::mutex& hudmutex){
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  //Don't bother with the HUD if we aren't connected via dbus
  while (hud_installed())
  {
    // if (hud_client == NULL) {
    //   return;
    // }
    hudmutex.lock();

    uint32_t diricon;
    if (navi_data->turn_event == 13) {
      diricon = roundabout(navi_data->turn_angle, navi_data->turn_side - 1);
    } else {
      int32_t turn_side = navi_data->turn_side - 1; //Google starts at 1 for some reason...
      diricon = turns[navi_data->turn_event][turn_side];
    }

    ::DBus::Struct< uint32_t, uint16_t, uint8_t, uint16_t, uint8_t, uint8_t > hudDisplayMsg;
    hudDisplayMsg._1 = diricon;
    hudDisplayMsg._2 = navi_data->distance;// distance;
    hudDisplayMsg._3 = navi_data->distance_unit;
    hudDisplayMsg._4 = 0; //Speed limit (Not Used)
    hudDisplayMsg._5 = 0; //Speed limit units (Not used)
    hudDisplayMsg._6 = navi_data->previous_msg;

    ::DBus::Struct< std::string, uint8_t > guidancePointData;
    guidancePointData._1 = navi_data->event_name;
    guidancePointData._2 = navi_data->previous_msg;

    if(navi_data->changed){
      try
      {
        vbsnavi_client->SetHUDDisplayMsgReq(hudDisplayMsg);
        tmc_client->SetHUD_Display_Msg2(guidancePointData);
      }
      catch(DBus::Error& error)
      {
        loge("DBUS: hud_send failed %s: %s\n", error.name(), error.message());
        return;
      }
	  navi_data->changed = 0;
    }
    hudmutex.unlock();
    {
        std::unique_lock<std::mutex> lk(quitmutex);
        if (quitcv.wait_for(lk, std::chrono::milliseconds(1000)) == std::cv_status::no_timeout)
        {
            break;
        }
    }
  }
}

void hud_start()
{
  if (hud_client != NULL)
    return;
    
  try
  {
    DBus::Connection service_bus(SERVICE_BUS_ADDRESS, false);
    service_bus.register_bus();
    DBus::Connection hmiBus(HMI_BUS_ADDRESS, false);
    hmiBus.register_bus();
    hud_client = new HUDSettingsClient(hmiBus, "/com/jci/navi2IHU", "com.jci.navi2IHU");
    vbsnavi_client = new NaviClient(service_bus, "/com/jci/vbs/navi", "com.jci.vbs.navi");
    tmc_client = new TMCClient(service_bus, "/com/jci/vbs/navi", "com.jci.vbs.navi");
  }
  catch(DBus::Error& error)
  {
    loge("DBUS: Failed to connect to SERVICE bus %s: %s\n", error.name(), error.message());
    hud_stop();
    return;
  }
  //logv("HUD dbus connections established\n");
  navi_data = new NaviData();
  return;
}

void hud_stop()
{
  delete hud_client;
  hud_client = nullptr;

  delete vbsnavi_client;
  vbsnavi_client = nullptr;

  delete tmc_client;
  tmc_client = nullptr;
}

bool hud_installed()
{
  if (hud_client == NULL)
      return(false);

  try
  {
      return(hud_client->GetHUDIsInstalled());
  }
  catch(DBus::Error& error)
  {
      loge("DBUS: GetHUDIsInstalled failed %s: %s\n", error.name(), error.message());
      return(false);
  }
}
