#include "callbacks.h"
#include "outputs.h"
#include "glib_utils.h"
#include "bt/ub_bluetooth.h"
#include "config.h"

DesktopEventCallbacks::DesktopEventCallbacks() :
    connected(false),
    videoFocus(false),
    audioFocus(false)
{
}

DesktopEventCallbacks::~DesktopEventCallbacks() {

}

int DesktopEventCallbacks::MediaPacket(int chan, uint64_t timestamp, const byte *buf, int len) {

    if (chan == AA_CH_VID && videoOutput) {
        videoOutput->MediaPacket(timestamp, buf, len);
    } else if (chan == AA_CH_AUD && audioOutput) {
        audioOutput->MediaPacketAUD(timestamp, buf, len);
    } else if (chan == AA_CH_AU1 && audioOutput) {
        audioOutput->MediaPacketAU1(timestamp, buf, len);
    }
    return 0;
}

int DesktopEventCallbacks::MediaStart(int chan) {
    if (chan == AA_CH_MIC) {
        printf("SHAI1 : Mic Started\n");
        micInput.Start(g_hu);
    }
    return 0;
}

int DesktopEventCallbacks::MediaStop(int chan) {
    if (chan == AA_CH_MIC) {
        micInput.Stop();
        printf("SHAI1 : Mic Stopped\n");
    }
    return 0;
}

void DesktopEventCallbacks::MediaSetupComplete(int chan) {
    if (chan == AA_CH_VID) {
        VideoFocusHappened(true, VIDEO_FOCUS_REQUESTOR::HEADUNIT);
    }
}

void DesktopEventCallbacks::DisconnectionOrError() {
    printf("DisconnectionOrError\n");
    g_main_loop_quit(gst_app.loop);
}

void DesktopEventCallbacks::CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel &streamChannel) {
#if ASPECT_RATIO_FIX
    if (chan == AA_CH_VID) {
        auto videoConfig = streamChannel.mutable_video_configs(0);
        videoConfig->set_margin_height(30);
    }
#endif
}

void DesktopEventCallbacks::AudioFocusRequest(int chan, const HU::AudioFocusRequest &request)  {
    run_on_main_thread([this, chan, request](){
        HU::AudioFocusResponse response;
        if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_RELEASE) {
            audioOutput.reset();
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_LOSS);
            audioFocus = false;
        } else {
            if (!audioOutput) {
                audioOutput.reset(new AudioOutput());
            }
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN);
            audioFocus = true;
        }

        g_hu->hu_queue_command([chan, response](IHUConnectionThreadInterface & s) {
            s.hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::AudioFocusResponse, response);
        });
        return false;
    });
}

void DesktopEventCallbacks::VideoFocusRequest(int chan, const HU::VideoFocusRequest &request) {
    VideoFocusHappened(request.mode() == HU::VIDEO_FOCUS_MODE_FOCUSED, VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO);
}

std::string DesktopEventCallbacks::GetCarBluetoothAddress()
{
    return get_bluetooth_mac_address();
}

void DesktopEventCallbacks::HandlePhoneStatus(IHUConnectionThreadInterface& stream, const HU::PhoneStatus& phoneStatus) {
    printf("HandlePhoneStatus: %s\n", phoneStatus.DebugString().c_str());
}

/*
void DesktopEventCallbacks::ShowingGenericNotifications(IHUConnectionThreadInterface& stream, bool bIsShowing) {
    printf("ShowingGenericNotifications: %s\n", bIsShowing ? "true" : "false");
}
*/

void DesktopEventCallbacks::VideoFocusHappened(bool hasFocus, VIDEO_FOCUS_REQUESTOR videoFocusRequestor) {
    run_on_main_thread([this, hasFocus, videoFocusRequestor](){
        if ((bool)videoOutput != hasFocus) {
            videoOutput.reset(hasFocus ? new VideoOutput(this) : nullptr);
        }
        videoFocus = hasFocus;
        bool unrequested = videoFocusRequestor != VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO;
        g_hu->hu_queue_command([hasFocus, unrequested](IHUConnectionThreadInterface & s) {
            HU::VideoFocus videoFocusGained;
            videoFocusGained.set_mode(hasFocus ? HU::VIDEO_FOCUS_MODE_FOCUSED : HU::VIDEO_FOCUS_MODE_UNFOCUSED);
            videoFocusGained.set_unrequested(unrequested);
            s.hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
        });
        return false;
    });
}

DesktopCommandServerCallbacks::DesktopCommandServerCallbacks()
{

}

bool DesktopCommandServerCallbacks::IsConnected() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->connected;
    }
    return false;
}

bool DesktopCommandServerCallbacks::HasAudioFocus() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->audioFocus;
    }
    return false;
}

bool DesktopCommandServerCallbacks::HasVideoFocus() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->videoFocus;
    }
    return false;
}

void DesktopCommandServerCallbacks::TakeVideoFocus()
{
    if (eventCallbacks && eventCallbacks->connected)
    {
        eventCallbacks->VideoFocusHappened(true, VIDEO_FOCUS_REQUESTOR::HEADUNIT);
    }
}

std::string DesktopCommandServerCallbacks::GetLogPath() const
{
    //no log
    return std::string();
}

std::string DesktopCommandServerCallbacks::GetVersion() const
{
    return HEADUNIT_VERSION;
}

std::string DesktopCommandServerCallbacks::ChangeParameterConfig(std::string param, std::string value, std::string type) const
{
    bool updateHappened = false;
    if (type == "string")
    {
        config::updateConfigString(param, value);
        updateHappened = true;
    }
    if (type == "bool")
    {
        if (value == "false")
        {
            config::updateConfigBool(param, false);
            updateHappened = true;
        }
        if (value == "true")
        {
            config::updateConfigBool(param, true);
            updateHappened = true;
        }
    }
    if (updateHappened)
       return "Config updated";
    return "Config wasn't updated. Wrong parameters.";
}

void DesktopEventCallbacks::HandleNaviStatus(IHUConnectionThreadInterface& stream, const HU::NAVMessagesStatus &request){
}

void DesktopEventCallbacks::HandleNaviTurn(IHUConnectionThreadInterface& stream, const HU::NAVTurnMessage &request){
    const char *event_name = &request.event_name()[0];
    std::string image = request.image();
    printf("%s", event_name);
    logv ("AA_CH_NAVI: %s, TurnSide: %d, TurnEvent:%d, TurnNumber: %d, TurnAngle: %d", event_name, request.turn_side(), request.turn_event(), request.turn_number(), request.turn_angle());
    hex_dump("AA_CH_NAVI", 256, (unsigned char*)image.c_str(), image.length());
}
void DesktopEventCallbacks::HandleNaviTurnDistance(IHUConnectionThreadInterface& stream, const HU::NAVDistanceMessage &request){
    logv ("AA_CH_NAVI: Distance: %d", request.distance());
}