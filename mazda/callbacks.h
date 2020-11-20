#pragma once

#include <atomic>
#include <set>
#include "hu_aap.h"

#include "command_server.h"
#include "audio.h"
#include <dbus-c++/dbus.h>
#include <alsa/asoundlib.h>

#include "dbus/generated_cmu.h"
#include "version.h"

class VideoOutput;
class AudioOutput;
class MazdaEventCallbacks;
class VideoManagerClient;

class NativeGUICtrlClient : public com::jci::nativeguictrl_proxy,
                            public DBus::ObjectProxy
{
public:
    NativeGUICtrlClient(DBus::Connection &connection)
            : DBus::ObjectProxy(connection, "/com/jci/nativeguictrl", "com.jci.nativeguictrl")
    {
    }

    enum SURFACES
    {
        NNG_NAVI_ID = 0,
        TV_TOUCH_SURFACE = 1,
        NATGUI_SURFACE = 2,
        LOOPLOGO_SURFACE = 3,
        TRANLOGOEND_SURFACE = 4,
        TRANLOGO_SURFACE = 5,
        QUICKTRANLOGO_SURFACE = 6,
        EXITLOGO_SURFACE = 8,
        JCI_OPERA_PRIMARY = 9,
        JCI_OPERA_SECONDARY = 10,
        lvdsSurface = 11,
        SCREENREP_IVI_NAME = 12,
        NNG_NAVI_MAP1 = 13,
        NNG_NAVI_MAP2 = 14,
        NNG_NAVI_HMI = 15,
        NNG_NAVI_TWN = 16,
    };

    void SetRequiredSurfacesByEnum(const int surface, bool fadeOpera)
    {
        SetRequiredSurfaces(std::to_string(surface), fadeOpera ? 1 : 0);
    }
};

class AudioManagerClient : public com::xsembedded::ServiceProvider_proxy,
                     public DBus::ObjectProxy
{
public:
    enum class FocusType
    {
        NONE,
        PERMANENT,
        TRANSIENT,
    };
private:
    std::map<std::string, int> streamToSessionIds;
    std::string aaStreamName = "MLENT";
    int aaSessionID = -1;
    int aaTransientSessionID = -1;
    int previousSessionID = -1;
    bool waitingForFocusLostEvent = false;
    MazdaEventCallbacks& callbacks;
    FocusType currentFocus = FocusType::NONE;

    //These IDs are usually the same, but they depend on the startup order of the services on the car so we can't assume them 100% reliably
    void populateStreamTable();
    void aaRegisterStream();
public:
    AudioManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection &connection);
    ~AudioManagerClient();

    bool canSwitchAudio();

    //calling requestAudioFocus directly doesn't work on the audio mgr
    void audioMgrRequestAudioFocus(FocusType type);
    void audioMgrReleaseAudioFocus();

    virtual void Notify(const std::string& signalName, const std::string& payload) override;
};

enum class VIDEO_FOCUS_REQUESTOR : u_int8_t {
    HEADUNIT, // headunit (we) has requested video focus
    ANDROID_AUTO, // AA phone app has requested video focus
    BACKUP_CAMERA // CMU requested screen for backup camera
};

class VideoManagerClient : public com::jci::bucpsa_proxy,
                           public DBus::ObjectProxy {
    bool allowedToGetFocus = true;
    bool waitsForFocus = false;

    MazdaEventCallbacks& callbacks;
    NativeGUICtrlClient guiClient;
public:
    VideoManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection &hmiBus);
    ~VideoManagerClient();

    void requestVideoFocus(VIDEO_FOCUS_REQUESTOR requestor);
    void releaseVideoFocus(VIDEO_FOCUS_REQUESTOR requestor);

    virtual void CommandResponse(const uint32_t& cmdResponse) override {}
    virtual void DisplayMode(const uint32_t& currentDisplayMode) override;
    virtual void ReverseStatusChanged(const int32_t& reverseStatus) override {}
    virtual void PSMInstallStatusChanged(const uint8_t& psmInstalled) override {}
    virtual void CameraType(const uint32_t& currentCameraType) override {}
    virtual void SteeringWheelLocation(const uint32_t& currentSteeringWheelLocation) override {}
};

class MazdaEventCallbacks : public IHUConnectionThreadEventCallbacks {
    std::unique_ptr<VideoOutput> videoOutput;
    std::unique_ptr<AudioOutput> audioOutput;

    MicInput micInput;
    DBus::Connection& serviceBus;
    DBus::Connection& hmiBus;

    std::unique_ptr<AudioManagerClient> audioMgrClient;
    std::unique_ptr<VideoManagerClient> videoMgrClient;
public:
    MazdaEventCallbacks(DBus::Connection& serviceBus, DBus::Connection& hmiBus);
    ~MazdaEventCallbacks();

    virtual int MediaPacket(int chan, uint64_t timestamp, const byte * buf, int len) override;
    virtual int MediaStart(int chan) override;
    virtual int MediaStop(int chan) override;
    virtual void MediaSetupComplete(int chan) override;
    virtual void DisconnectionOrError() override;
    virtual void CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel& streamChannel) override;
    virtual void AudioFocusRequest(int chan, const HU::AudioFocusRequest& request) override;
    virtual void VideoFocusRequest(int chan, const HU::VideoFocusRequest& request) override;

    virtual std::string GetCarBluetoothAddress() override;

    void takeVideoFocus();
    void releaseVideoFocus();
    void releaseAudioFocus();

    void VideoFocusHappened(bool hasFocus, bool unrequested);
    void AudioFocusHappend(AudioManagerClient::FocusType type);

    void HandlePhoneStatus(IHUConnectionThreadInterface& stream, const HU::PhoneStatus& phoneStatus) override;

    std::atomic<bool> connected;
    std::atomic<bool> videoFocus;
    std::atomic<bool> inCall;
    std::atomic<AudioManagerClient::FocusType> audioFocus;

    virtual void HandleNaviStatus(IHUConnectionThreadInterface& stream, const HU::NAVMessagesStatus &request) override;
    virtual void HandleNaviTurn(IHUConnectionThreadInterface& stream, const HU::NAVTurnMessage &request) override;
    virtual void HandleNaviTurnDistance(IHUConnectionThreadInterface& stream, const HU::NAVDistanceMessage &request) override;
};

class MazdaCommandServerCallbacks : public ICommandServerCallbacks
{
public:
    MazdaCommandServerCallbacks();

    MazdaEventCallbacks* eventCallbacks = nullptr;

    virtual bool IsConnected() const override;
    virtual bool HasAudioFocus() const override;
    virtual bool HasVideoFocus() const override;
    virtual void TakeVideoFocus() override;
    virtual std::string GetLogPath() const override;
    virtual std::string GetVersion() const override;
    virtual std::string ChangeParameterConfig(std::string param, std::string value, std::string type) const override;
};
