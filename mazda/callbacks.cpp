#include "callbacks.h"
#include "outputs.h"
#include "glib_utils.h"
#include "audio.h"
#include "main.h"
#include "bt/mzd_bluetooth.h"
#include "hud/hud.h"
#include "config.h"

#include "json/json.hpp"
using json = nlohmann::json;

MazdaEventCallbacks::MazdaEventCallbacks(DBus::Connection& serviceBus, DBus::Connection& hmiBus)
    : micInput("mic")
    , serviceBus(serviceBus)
    , hmiBus(hmiBus)
    , connected(false)
    , videoFocus(false)
    , inCall(false)
    , audioFocus(AudioManagerClient::FocusType::NONE)
{
    //no need to create/destroy this
    audioOutput.reset(new AudioOutput("entertainmentMl"));
    audioMgrClient.reset(new AudioManagerClient(*this, serviceBus));
    videoMgrClient.reset(new VideoManagerClient(*this, hmiBus));
}

MazdaEventCallbacks::~MazdaEventCallbacks() {

}

int MazdaEventCallbacks::MediaPacket(int chan, uint64_t timestamp, const byte *buf, int len) {

    if (chan == AA_CH_VID && videoOutput) {
        videoOutput->MediaPacket(timestamp, buf, len);
    } else if (chan == AA_CH_AUD && audioOutput) {
        audioOutput->MediaPacketAUD(timestamp, buf, len);
    } else if (chan == AA_CH_AU1 && audioOutput) {
        audioOutput->MediaPacketAU1(timestamp, buf, len);
    }
    return 0;
}

int MazdaEventCallbacks::MediaStart(int chan) {
    if (chan == AA_CH_MIC) {
        printf("SHAI1 : Mic Started\n");
        micInput.Start(g_hu);
    }
    return 0;
}

int MazdaEventCallbacks::MediaStop(int chan) {
    if (chan == AA_CH_MIC) {
        micInput.Stop();
        printf("SHAI1 : Mic Stopped\n");
    }
    return 0;
}

void MazdaEventCallbacks::MediaSetupComplete(int chan) {
    if (chan == AA_CH_VID) {
        run_on_main_thread([this](){
            //Ask for video focus on connection
            videoMgrClient->requestVideoFocus(VIDEO_FOCUS_REQUESTOR::HEADUNIT);
            return false;
        });
    }
}

void MazdaEventCallbacks::DisconnectionOrError() {
    printf("DisconnectionOrError\n");
    if(g_main_loop_is_running(gst_app.loop)){
        g_main_loop_quit(gst_app.loop);
    }
}

void MazdaEventCallbacks::CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel &streamChannel) {
#if ASPECT_RATIO_FIX
    if (chan == AA_CH_VID) {
        auto videoConfig = streamChannel.mutable_video_configs(0);
        videoConfig->set_margin_height(30);
    }
#endif
}

void MazdaEventCallbacks::releaseAudioFocus()  {
    run_on_main_thread([this](){
        audioMgrClient->audioMgrReleaseAudioFocus();
        return false;
    });
}
void MazdaEventCallbacks::AudioFocusRequest(int chan, const HU::AudioFocusRequest &request)  {

    run_on_main_thread([this, request](){
        //The chan passed here is always AA_CH_CTR but internally we pass the channel AA means
        if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_RELEASE) {
            audioMgrClient->audioMgrReleaseAudioFocus();
        } else {
            if (!inCall) {
                if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_GAIN_TRANSIENT) { // || request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_GAIN_NAVI) {
                    audioMgrClient->audioMgrRequestAudioFocus(AudioManagerClient::FocusType::TRANSIENT); //assume media
                } else if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_GAIN) {
                    audioMgrClient->audioMgrRequestAudioFocus(AudioManagerClient::FocusType::PERMANENT); //assume media
                }
            } else {
                logw("Tried to request focus %i but was in a call", (int)request.focus_type());
            }
        }

        return false;
    });
}

void MazdaEventCallbacks::VideoFocusRequest(int chan, const HU::VideoFocusRequest &request) {
    run_on_main_thread([this, request](){
        if (request.mode() == HU::VIDEO_FOCUS_MODE::VIDEO_FOCUS_MODE_FOCUSED) {
            videoMgrClient->requestVideoFocus(VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO);
        } else {
            videoMgrClient->releaseVideoFocus(VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO);
        }
        return false;
    });
}

std::string MazdaEventCallbacks::GetCarBluetoothAddress()
{
    return get_bluetooth_mac_address();
}

void MazdaEventCallbacks::takeVideoFocus() {
    run_on_main_thread([this](){
        videoMgrClient->requestVideoFocus(VIDEO_FOCUS_REQUESTOR::HEADUNIT);
        return false;
    });
}

void MazdaEventCallbacks::releaseVideoFocus() {
    run_on_main_thread([this](){
        videoMgrClient->releaseVideoFocus(VIDEO_FOCUS_REQUESTOR::HEADUNIT);
        return false;
    });
}

void MazdaEventCallbacks::VideoFocusHappened(bool hasFocus, bool unrequested) {
    videoFocus = hasFocus;
    if ((bool)videoOutput != hasFocus) {
        videoOutput.reset(hasFocus ? new VideoOutput(this) : nullptr);
    }
    g_hu->hu_queue_command([hasFocus, unrequested] (IHUConnectionThreadInterface & s) {
        HU::VideoFocus videoFocusGained;
        videoFocusGained.set_mode(hasFocus ? HU::VIDEO_FOCUS_MODE_FOCUSED : HU::VIDEO_FOCUS_MODE_UNFOCUSED);
        videoFocusGained.set_unrequested(unrequested);
        s.hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
    });
}

void MazdaEventCallbacks::AudioFocusHappend(AudioManagerClient::FocusType type) {
    printf("AudioFocusHappend(%i)\n", int(type));
    audioFocus = type;
    HU::AudioFocusResponse response;
    switch(type) {
        case AudioManagerClient::FocusType::NONE:
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_LOSS);
            break;
        case AudioManagerClient::FocusType::PERMANENT:
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN);
            break;
        case AudioManagerClient::FocusType::TRANSIENT:
            response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN_TRANSIENT);
            break;
    }
    g_hu->hu_queue_command([response](IHUConnectionThreadInterface & s) {
        s.hu_aap_enc_send_message(0, AA_CH_CTR, HU_PROTOCOL_MESSAGE::AudioFocusResponse, response);
    });
    logd("Sent channel %i HU_PROTOCOL_MESSAGE::AudioFocusResponse %s\n", AA_CH_CTR,  HU::AudioFocusResponse::AUDIO_FOCUS_STATE_Name(response.focus_type()).c_str());
}

void MazdaEventCallbacks::HandlePhoneStatus(IHUConnectionThreadInterface& stream, const HU::PhoneStatus& phoneStatus) {
    inCall = phoneStatus.calls_size() > 0;
}

VideoManagerClient::VideoManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection& hmiBus)
        : DBus::ObjectProxy(hmiBus, "/com/jci/bucpsa", "com.jci.bucpsa"), guiClient(hmiBus), callbacks(callbacks)
{
    uint32_t currentDisplayMode;
    int32_t returnValue;
    // check if backup camera is not visible at the moment and get output only when not
    GetDisplayMode(currentDisplayMode, returnValue);
    allowedToGetFocus = !(bool)currentDisplayMode;
}

VideoManagerClient::~VideoManagerClient() {
    //We can't call release video focus since the callbacks object is being destroyed, but make sure we got to opera if no in backup cam
    if (allowedToGetFocus) {
        logd("Requesting video surface: JCI_OPERA_PRIMARY");
        guiClient.SetRequiredSurfacesByEnum(NativeGUICtrlClient::JCI_OPERA_PRIMARY, true);
    }
}

void VideoManagerClient::requestVideoFocus(VIDEO_FOCUS_REQUESTOR requestor)
{
    if (!allowedToGetFocus) {
        // we can safely exit - backup camera will notice us when finish and we request focus back
        waitsForFocus = true;
        return;
    }
    waitsForFocus = false;
    bool unrequested = requestor != VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO;
    logd("Requestor %i requested video focus\n", requestor);

    auto handleRequest = [this, unrequested](){
        callbacks.VideoFocusHappened(true, unrequested);
        logd("Requesting video surface: TV_TOUCH_SURFACE");
        guiClient.SetRequiredSurfacesByEnum(NativeGUICtrlClient::TV_TOUCH_SURFACE, true);
        return false;
    };
    if (requestor == VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA)
    {
        // need to wait for a second (maybe less but 100ms is too early) to make sure
        // the CMU has already changed the surface from backup camera to opera
        run_on_main_thread_delay(1000, handleRequest);
    }
    else
    {
        //otherwise don't pause
        handleRequest();
    }
}

void VideoManagerClient::releaseVideoFocus(VIDEO_FOCUS_REQUESTOR requestor)
{
    if (!callbacks.videoFocus) {
        return;
    }
    bool unrequested = requestor != VIDEO_FOCUS_REQUESTOR::ANDROID_AUTO;
    logd("Requestor %i released video focus\n", requestor);
    callbacks.VideoFocusHappened(false, unrequested);
    if (requestor != VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA) {
        logd("Requesting video surface: JCI_OPERA_PRIMARY");
        guiClient.SetRequiredSurfacesByEnum(NativeGUICtrlClient::JCI_OPERA_PRIMARY, true);
    }
}

void VideoManagerClient::DisplayMode(const uint32_t &currentDisplayMode)
{
    // currentDisplayMode != 0 means backup camera wants the screen
    allowedToGetFocus = !(bool)currentDisplayMode;
    if ((bool)currentDisplayMode) {
        waitsForFocus = callbacks.videoFocus;
        releaseVideoFocus(VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA);
    } else if (waitsForFocus) {
        requestVideoFocus(VIDEO_FOCUS_REQUESTOR::BACKUP_CAMERA);
    }
}

MazdaCommandServerCallbacks::MazdaCommandServerCallbacks()
{

}

bool MazdaCommandServerCallbacks::IsConnected() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->connected;
    }
    return false;
}

bool MazdaCommandServerCallbacks::HasAudioFocus() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->audioFocus != AudioManagerClient::FocusType::NONE;
    }
    return false;
}

bool MazdaCommandServerCallbacks::HasVideoFocus() const
{
    if (eventCallbacks)
    {
        return eventCallbacks->videoFocus;
    }
    return false;
}

void MazdaCommandServerCallbacks::TakeVideoFocus()
{
    if (eventCallbacks && eventCallbacks->connected)
    {
        eventCallbacks->takeVideoFocus();
    }
}

std::string MazdaCommandServerCallbacks::GetLogPath() const
{
    return "/tmp/mnt/data/headunit.log";
}

std::string MazdaCommandServerCallbacks::GetVersion() const
{
    return HEADUNIT_VERSION;
}

std::string MazdaCommandServerCallbacks::ChangeParameterConfig(std::string param, std::string value, std::string type) const
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

void AudioManagerClient::aaRegisterStream()
{
    // First open a new Stream
    json sessArgs = {
        { "busName", "com.jci.usbm_am_client" },
        { "objectPath", "/com/jci/usbm_am_client" },
        { "destination", "Cabin" }
    };
    if (aaSessionID < 0)
    {
        try
        {
            std::string sessString = Request("openSession", sessArgs.dump());
            printf("openSession(%s)\n%s\n", sessArgs.dump().c_str(), sessString.c_str());
            aaSessionID = json::parse(sessString)["sessionId"];

            // Register the stream
            json regArgs = {
                { "sessionId", aaSessionID },
                { "streamName", aaStreamName },
                // { "streamModeName", aaStreamName },
                { "focusType", "permanent" },
                { "streamType", "Media" }
            };
            std::string regString = Request("registerAudioStream", regArgs.dump());
            printf("registerAudioStream(%s)\n%s\n", regArgs.dump().c_str(), regString.c_str());
        }
        catch (const std::domain_error& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }
        catch (const std::invalid_argument& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }

        // Stream is registered add it to the array
        streamToSessionIds[aaStreamName] = aaSessionID;
    }

    if (aaTransientSessionID < 0)
    {
        try
        {
            std::string sessString = Request("openSession", sessArgs.dump());
            printf("openSession(%s)\n%s\n", sessArgs.dump().c_str(), sessString.c_str());
            aaTransientSessionID = json::parse(sessString)["sessionId"];

            // Register the stream
            json regArgs = {
                { "sessionId", aaTransientSessionID },
                { "streamName", aaStreamName },
                // { "streamModeName", aaStreamName },
                { "focusType", "transient" },
                { "streamType", "InfoUser" }
            };
            std::string regString = Request("registerAudioStream", regArgs.dump());
            printf("registerAudioStream(%s)\n%s\n", regArgs.dump().c_str(), regString.c_str());
        }
        catch (const std::domain_error& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }
        catch (const std::invalid_argument& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }

        // Stream is registered add it to the array
        streamToSessionIds[aaStreamName] = aaTransientSessionID;
    }


}
void AudioManagerClient::populateStreamTable()
{
    streamToSessionIds.clear();
    json requestArgs = {
        { "svc", "SRCS" },
        { "pretty", false }
    };
    std::string resultString = Request("dumpState", requestArgs.dump());
    printf("dumpState(%s)\n%s\n", requestArgs.dump().c_str(), resultString.c_str());
    /*
         * An example resonse:
         *
        {
          "HMI": {

          },
          "APP": [
            "1.Media.Pandora.granted.NotPlaying",
            "2.Media.AM..NotPlaying"
          ]
        }
        */
    //Row format:
    //"%d.%s.%s.%s.%s", obj.sessionId, obj.stream.streamType, obj.stream.streamName, obj.focus, obj.stream.playing and "playing" or "NotPlaying")

    try
    {
        auto result = json::parse(resultString);
        for (auto& sessionRecord : result["APP"].get_ref<json::array_t&>())
        {
            std::string sessionStr = sessionRecord.get<std::string>();
            //Stream names have no spaces so it's safe to do this
            std::replace(sessionStr.begin(), sessionStr.end(), '.', ' ');
            std::istringstream sessionIStr(sessionStr);

            int sessionId;
            std::string streamName, streamType;

            if (!(sessionIStr >> sessionId >> streamType >> streamName))
            {
                logw("Can't parse line \"%s\"", sessionRecord.get<std::string>().c_str());
                continue;
            }

            printf("Found stream %s session id %i\n", streamName.c_str(), sessionId);
            if(streamName == aaStreamName)
            {
                if (aaSessionID < 0)
                    aaSessionID = sessionId;
                else
                    aaTransientSessionID = sessionId;
            }
            else
            {
                //We have two so this doesn't work
                streamToSessionIds[streamName] = sessionId;
            }
        }
        // Create and register stream (only if we need to)
        if (aaSessionID < 0 || aaTransientSessionID < 0)
        {
            aaRegisterStream();
        }
    }
    catch (const std::domain_error& ex)
    {
        loge("Failed to parse state json: %s", ex.what());
        printf("%s\n", resultString.c_str());
    }
    catch (const std::invalid_argument& ex)
    {
        loge("Failed to parse state json: %s", ex.what());
        printf("%s\n", resultString.c_str());
    }
}

AudioManagerClient::AudioManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection &connection)
    : DBus::ObjectProxy(connection, "/com/xse/service/AudioManagement/AudioApplication", "com.xsembedded.service.AudioManagement")
    , callbacks(callbacks)
{
    populateStreamTable();
    if (aaSessionID < 0 || aaTransientSessionID < 0)
    {
        loge("Can't find audio stream. Audio will not work");
    }
}

AudioManagerClient::~AudioManagerClient()
{
    if (currentFocus != FocusType::NONE && previousSessionID >= 0)
    {
        json args = { { "sessionId", previousSessionID } };
        std::string result = Request("requestAudioFocus", args.dump());
        printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
    }

    for (int session : {aaSessionID, aaTransientSessionID })
    {
        if (session >= 0)
        {
            json args = { { "sessionId", session } };
            std::string result = Request("closeSession", args.dump());
            printf("closeSession(%s)\n%s\n", args.dump().c_str(), result.c_str());
        }
    }
}

bool AudioManagerClient::canSwitchAudio() { return aaSessionID >= 0 && aaTransientSessionID >= 0; }

void AudioManagerClient::audioMgrRequestAudioFocus(FocusType type)
{
    if (type == FocusType::NONE)
    {
        audioMgrReleaseAudioFocus();
        return;
    }
    printf("audioMgrRequestAudioFocus(%i)\n", int(type));
    if (currentFocus == type)
    {
        callbacks.AudioFocusHappend(currentFocus);
        return;
    }

    if (currentFocus == FocusType::NONE && type == FocusType::PERMANENT)
    {
        waitingForFocusLostEvent = true;
        previousSessionID = -1;
    }
    json args = { { "sessionId", type == FocusType::TRANSIENT ? aaTransientSessionID : aaSessionID } };
    std::string result = Request("requestAudioFocus", args.dump());
    printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
}

void AudioManagerClient::audioMgrReleaseAudioFocus()
{
    printf("audioMgrReleaseAudioFocus()\n");
    if (currentFocus == FocusType::NONE)
    {
        //nothing to do
        callbacks.AudioFocusHappend(currentFocus);
    }
    else if (currentFocus == FocusType::PERMANENT && previousSessionID >= 0)
    {
        //We released the last one, give up audio focus for real
        json args = { { "sessionId", previousSessionID } };
        std::string result = Request("requestAudioFocus", args.dump());
        printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
        previousSessionID = -1;
    }
    else if (currentFocus == FocusType::TRANSIENT)
    {
        json args = { { "sessionId", aaTransientSessionID } };
        std::string result = Request("abandonAudioFocus", args.dump());
        printf("abandonAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
        previousSessionID = -1;
    }
    else
    {
        currentFocus = FocusType::NONE;
        callbacks.AudioFocusHappend(currentFocus);
    }
}

void AudioManagerClient::Notify(const std::string &signalName, const std::string &payload)
{
    printf("AudioManagerClient::Notify signalName=%s payload=%s\n", signalName.c_str(), payload.c_str());
    if (signalName == "audioFocusChangeEvent")
    {
        try
        {
            auto result = json::parse(payload);
            std::string streamName = result["streamName"].get<std::string>();
            std::string newFocus = result["newFocus"].get<std::string>();
            std::string focusType = result["focusType"].get<std::string>();

            int eventSessionID = -1;
            if (streamName == aaStreamName)
            {
                if (focusType == "permanent")
                {
                    eventSessionID = aaSessionID;
                }
                else
                {
                    eventSessionID = aaTransientSessionID;
                }
                logd("Found audio sessionId %i for stream %s\n", eventSessionID, streamName.c_str());
            }
            else
            {
                auto findIt = streamToSessionIds.find(streamName);
                if (findIt != streamToSessionIds.end())
                {
                    eventSessionID = findIt->second;
                    logd("Found audio sessionId %i for stream %s with focusType %s & newFocus %s\n", eventSessionID, streamName.c_str(), focusType.c_str(), newFocus.c_str());
                }
                else
                {
                    loge("Can't find audio sessionId for stream %s\n", streamName.c_str());
                }
            }

            if (eventSessionID >= 0)
            {
                if (waitingForFocusLostEvent && newFocus == "lost")
                {
                    previousSessionID = eventSessionID;
                    waitingForFocusLostEvent = false;
                }

                FocusType newFocusType = currentFocus;
                if (newFocus != "gained")
                {
                    if (eventSessionID == aaSessionID || eventSessionID == aaTransientSessionID)
                    {
                        newFocusType = FocusType::NONE;
                    }
                }
                else
                {
                    if (eventSessionID == aaTransientSessionID)
                    {
                        newFocusType = FocusType::TRANSIENT;
                    }
                    else if (eventSessionID == aaSessionID)
                    {
                        newFocusType = FocusType::PERMANENT;
                    }
                }

                if (currentFocus != newFocusType)
                {
                    currentFocus = newFocusType;
                    callbacks.AudioFocusHappend(currentFocus);
                }
            }
        }
        catch (const std::domain_error& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }
        catch (const std::invalid_argument& ex)
        {
            loge("Failed to parse state json: %s", ex.what());
        }
    }
}

extern NaviData *navi_data;
extern std::mutex hudmutex;

void MazdaEventCallbacks::HandleNaviStatus(IHUConnectionThreadInterface& stream, const HU::NAVMessagesStatus &request){
    if (navi_data == nullptr){
        return;
    }
  if (request.status() == HU::NAVMessagesStatus_STATUS_STOP) {
    hudmutex.lock();
    navi_data->event_name = "";
    navi_data->turn_event = 0;
    navi_data->turn_side = 0;
    navi_data->turn_number = -1;
    navi_data->turn_angle = -1;
    navi_data->changed = 1;
    navi_data->previous_msg = navi_data->previous_msg + 1;
    if (navi_data->previous_msg == 8){
      navi_data->previous_msg = 1;
    }
    hudmutex.unlock();
  }
}


void MazdaEventCallbacks::HandleNaviTurn(IHUConnectionThreadInterface& stream, const HU::NAVTurnMessage &request){
    if (navi_data == nullptr){
        return;
    }
  logw("NAVTurnMessage: turn_side: %d, turn_event: %d, turn_number: %d, turn_angle: %d, event_name: %s", 
      request.turn_side(),
      request.turn_event(),
      request.turn_number(),
      request.turn_angle(),
      request.event_name().c_str()
  );

  hudmutex.lock();
  int changed = 0;
  if (navi_data->event_name != request.event_name()) {
    navi_data->event_name = request.event_name();
    changed = 1;
  }
  if (navi_data->turn_event != request.turn_event()) {
    navi_data->turn_event = request.turn_event();
    changed = 1;
  }
  if (navi_data->turn_side != request.turn_side()) {
    navi_data->turn_side = request.turn_side();
    changed = 1;
  }
  if (navi_data->turn_number != request.turn_number()) {
    navi_data->turn_number = request.turn_number();
    changed = 1;
  }
  if (navi_data->turn_angle != request.turn_angle()) {
    navi_data->turn_angle = request.turn_angle();
    changed = 1;
  }
  if (changed) {
    navi_data->changed = 1;
    navi_data->previous_msg = navi_data->previous_msg+1;
    if (navi_data->previous_msg == 8) {
      navi_data->previous_msg = 1;
    }
  }
  hudmutex.unlock();
}

void MazdaEventCallbacks::HandleNaviTurnDistance(IHUConnectionThreadInterface& stream, const HU::NAVDistanceMessage &request) {
    if (navi_data == nullptr){
        return;
    }
  hudmutex.lock();
  int now_distance;
  HudDistanceUnit now_unit;
  switch (request.display_distance_unit()) {
      case HU::NAVDistanceMessage_DISPLAY_DISTANCE_UNIT_METERS:
        now_distance = request.display_distance() / 100;
        now_unit = HudDistanceUnit::METERS;
        break;
      case HU::NAVDistanceMessage_DISPLAY_DISTANCE_UNIT_KILOMETERS10:
      case HU::NAVDistanceMessage_DISPLAY_DISTANCE_UNIT_KILOMETERS:
        now_distance = request.display_distance() / 100;
        now_unit = HudDistanceUnit::KILOMETERS;
        break;
      case HU::NAVDistanceMessage_DISPLAY_DISTANCE_UNIT_MILES10:
      case HU::NAVDistanceMessage_DISPLAY_DISTANCE_UNIT_MILES:
        now_distance = request.display_distance() / 100;
        now_unit = HudDistanceUnit::MILES;
        break;
      case HU::NAVDistanceMessage_DISPLAY_DISTANCE_UNIT_FEET:
        now_distance = request.display_distance() / 100;
        now_unit = HudDistanceUnit::FEET;
        break;
      default: //not sure, use SI and log
        logw("NAVDistanceMessage: distance: %d, time: %d, display_distance: %u, display_distance_unit: %d", 
            request.distance(),
            request.time_until(),
            request.display_distance(),
            request.display_distance_unit()
        );

        if (request.distance() > 1000) {
            now_distance = request.distance() / 100;
            now_unit = HudDistanceUnit::KILOMETERS;
        } else {
            now_distance = (((request.distance() + 5) / 10) * 10) * 10;
            now_unit = HudDistanceUnit::METERS;
        }
  }
  
  if (now_distance != navi_data->distance || now_unit != navi_data->distance_unit) {
    navi_data->distance_unit = now_unit;
    navi_data->distance = now_distance;
    navi_data->changed = 1;
    navi_data->previous_msg = navi_data->previous_msg+1;
    if (navi_data->previous_msg == 8) {
      navi_data->previous_msg = 1;
    }
  }

  if (navi_data->time_until != request.time_until()) {
    navi_data->time_until = request.time_until();
    navi_data->changed = 1;
    navi_data->previous_msg = navi_data->previous_msg+1;
    if (navi_data->previous_msg == 8) {
      navi_data->previous_msg = 1;
    }
  }

  hudmutex.unlock();
}
