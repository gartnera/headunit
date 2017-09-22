#include "mzd_nightmode.h"

#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include "../dbus/generated_cmu.h"

#define LOGTAG "mazda-nm"

#include "hu_uti.h"

#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

class Navi2NNGClient : public com::jci::navi2NNG_proxy,
                     public DBus::ObjectProxy
{
public:
    Navi2NNGClient(DBus::Connection &connection, const char *path, const char *name)
        : DBus::ObjectProxy(connection, path, name)
    {
    }

    virtual void ShutdownRequest() override {}
    virtual void FactoryReset() override {}
    virtual void ShowNavigation() override {}
    virtual void RequestShowNavigationDenied() override {}
    virtual void ClearStack() override {}
    virtual void FavoriteLongPress() override {}
    virtual void NaviButtonPress() override {}
    virtual void GuiFocusStatusUpdate(const int32_t& status) override {}
    virtual void AudioDone(const int32_t& callbackId, const int32_t& result) override {}
    virtual void SetNavigationVolume(const int32_t& volume) override {}
    virtual void VREvent(const std::string& eventId) override {}
    virtual void SelectedListItem(const int32_t& seletedItem) override {}
    virtual void NavigateToPOI(const ::DBus::Struct< int32_t, std::vector< ::DBus::Struct< std::string > > >& poiCategoryName) override {}
    virtual void AddWaypointPOI(const ::DBus::Struct< int32_t, std::vector< ::DBus::Struct< std::string > > >& poiCategoryName) override {}
    virtual void JpjIntermediateHypothesis(const ::DBus::Struct< uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t >& jpjHypothesis) override {}
    virtual void JpjVDEHypothesisList(const ::DBus::Struct< int32_t, std::vector< ::DBus::Struct< uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t > > >& jpjHypothesisList) override {}
    virtual void VDEHypothesisList(const ::DBus::Struct< int32_t, std::vector< ::DBus::Struct< uint32_t, uint32_t, uint32_t, uint32_t > > >& vDEList) override {}
    virtual void SimpleHypothesisList(const ::DBus::Struct< int32_t, std::vector< ::DBus::Struct< uint32_t > > >& idList) override {}
    virtual void ModeChanged(const int32_t& modality) override {}
    virtual void RequestGuidanceInfo() override {}
    virtual void NavigateToAddress(const std::string& name, const std::string& countryName, const std::string& stateName, const std::string& cityName, const std::string& streetName, const std::string& zipCode, const double& latitude, const double& longitude) override {}
    virtual void DeleteFavorite(const uint32_t& iD) override {}
    virtual void DeleteAllFavorites() override {}
    virtual void NavigateToFavorite(const uint32_t& iD) override {}
    virtual void RenameFavorite(const uint32_t& iD, const std::string& name) override {}
    virtual void SetHome() override {}
    virtual void UnsetHome() override {}
    virtual void SwapFavorites(const uint32_t& iD1, const uint32_t& iD2) override {}
    virtual void MoveFavorite(const uint32_t& iD1, const uint32_t& iD2) override {}
    virtual void AddCurrentPositionToFavorites() override {}
    virtual void ReplaceFavoriteWithCurrentPosition(const uint32_t& iD) override {}
    virtual void AddCurrentDestinationToFavorites() override {}
    virtual void ReplaceFavoriteWithCurrentDestination(const uint32_t& iD) override {}
    virtual void AddFavorite(const std::string& name, const std::string& countryName, const std::string& stateName, const std::string& cityName, const std::string& streetName, const std::string& zipCode, const double& latitude, const double& longitude) override {}
    virtual void ReplaceFavoriteWithAddress(const uint32_t& iD, const std::string& name, const std::string& countryName, const std::string& stateName, const std::string& cityName, const std::string& streetName, const std::string& zipCode, const double& latitude, const double& longitude) override {}
};


static DBus::Connection *service_bus = NULL;
static Navi2NNGClient *navi_client = NULL;

void mzd_nightmode_start() {
    if (service_bus != NULL)
        return;

    try
    {
        service_bus = new DBus::Connection(SERVICE_BUS_ADDRESS, false);
        service_bus->register_bus();
        navi_client = new Navi2NNGClient(*service_bus, "/com/jci/navi2NNG", "com.jci.navi2NNG");
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: Failed to connect to SERVICE bus %s: %s", error.name(), error.message());
        delete navi_client;
        delete service_bus;
        navi_client = nullptr;
        service_bus = nullptr;
        return;
    }

    printf("Nightmode service connection established.\n");
}

int mzd_is_night_mode_set() {
    if (service_bus == NULL) return
            NM_NO_VALUE;

    try
    {
        int32_t nm_daynightmode = navi_client->GetDayNightMode();
        return (nm_daynightmode == 1) ? NM_NIGHT_MODE : NM_DAY_MODE;
    }
    catch(DBus::Error& error)
    {
        loge("DBUS: GetDayNightMode failed %s: %s", error.name(), error.message());
        return NM_NO_VALUE;
    }
}

void mzd_nightmode_stop() {
    delete navi_client;
    delete service_bus;
    navi_client = nullptr;
    service_bus = nullptr;
}
