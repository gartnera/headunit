#pragma once

#include "hu_uti.h"
#include "hu.pb.h"

int hu_aap_mic_get ();
int hu_aap_out_get (int chan);

// Channels ( or Service IDs)
#define AA_CH_CTR 0                                                                                  // Sync with hu_tra.java, hu_aap.h and hu_aap.c:aa_type_array[]
#define AA_CH_TOU 1
#define AA_CH_SEN 2
#define AA_CH_VID 3
#define AA_CH_AUD 4
#define AA_CH_AU1 5
#define AA_CH_AU2 6
#define AA_CH_MIC 7
#define AA_CH_MAX 7


int hu_aap_tra_recv (byte * buf, int len, int tmo);                      // Used by intern,                      hu_ssl
int hu_aap_tra_set  (int chan, int flags, int type, byte * buf, int len);// Used by intern                       hu_ssl
int hu_aap_tra_send (int retry, byte * buf, int len, int tmo);                      // Used by intern,                      hu_ssl
int hu_aap_enc_send (int retry, int chan, byte * buf, int len);                     // Used by intern,            hu_jni     // Encrypted Send
int hu_aap_stop     ();                                                  // Used by          hu_mai,  hu_jni     // NEED: Send Byebye, Stops USB/ACC/OAP
int hu_aap_start    (byte ep_in_addr, byte ep_out_addr);                 // Used by          hu_mai,  hu_jni     // Starts USB/ACC/OAP, then AA protocol w/ VersReq(1), SSL handshake, Auth Complete
int hu_aap_recv_process ();                                              // Used by          hu_mai,  hu_jni     // Process 1 encrypted receive message set:
                                                                                                                        // Read encrypted message from USB
                                                                                                                        // Respond to decrypted message
int hu_aap_enc_send_message(int retry, int chan, uint16_t messageCode, const google::protobuf::MessageLite& message);

template<typename EnumType>
inline int hu_aap_enc_send_message(int retry, int chan, EnumType messageCode, const google::protobuf::MessageLite& message)
{
  return hu_aap_enc_send_message(retry, chan, static_cast<uint16_t>(messageCode), message);
}

int hu_aap_enc_send_media_packet(int retry, int chan, uint16_t messageCode, uint64_t timeStamp, const byte* buffer, int bufferLen);

template<typename EnumType>
inline int hu_aap_enc_send_media_packet(int retry, int chan, EnumType messageCode, uint64_t timeStamp, const byte* buffer, int bufferLen)
{
  return hu_aap_enc_send_media_packet(retry, chan, static_cast<uint16_t>(messageCode), timeStamp, buffer, bufferLen);
}


enum class HU_PROTOCOL_MESSAGE : uint16_t
{
  MediaData0 = 0x0000,
  MediaData1 = 0x0001,
  VersionResponse = 0x0002,
  SSLHandshake = 0x0003,
  ServiceDiscoveryRequest = 0x0005,
  ServiceDiscoveryResponse = 0x0006,    
  ChannelOpenRequest = 0x0007,
  ChannelOpenResponse = 0x0008,
  PingRequest = 0x000b,
  PingResponse = 0x000c,
  NavigationFocusRequest = 0x000d,
  NavigationFocusResponse = 0x000e,
  ShutdownRequest = 0x000f,
  ShutdownResponse = 0x0010,
  VoiceSessionRequest = 0x0011,
  AudioFocusRequest = 0x0012,
  AudioFocusResponse = 0x0013,
};                                                                                                                           // If video data, put on queue

enum class HU_MEDIA_CHANNEL_MESSAGE : uint16_t
{
  MediaSetupRequest = 0x8000, //Setup
  MediaStartRequest = 0x8001, //Start
  MediaStopRequest = 0x8002, //Stop
  MediaSetupResponse = 0x8003, //Config
  MediaAck = 0x8004,
  MicRequest = 0x8005,
  MicReponse = 0x8006,
  VideoFocusRequest = 0x8007,
  VideoFocus = 0x8008,
};    

enum class HU_SENSOR_CHANNEL_MESSAGE : uint16_t
{
  SensorStartRequest = 0x8001,
  SensorStartResponse = 0x8002,
  SensorEvent = 0x8003,
};    

enum class HU_INPUT_CHANNEL_MESSAGE : uint16_t
{
  InputEvent = 0x8001,
  BindingRequest = 0x8002,
  BindingResponse = 0x8003,
};    

enum HU_INPUT_BUTTON
{
    HUIB_UP = 0x13,
    HUIB_DOWN = 0x14,
    HUIB_LEFT = 0x15,
    HUIB_RIGHT = 0x16,
    HUIB_BACK = 0x04,
    HUIB_ENTER = 0x17,
    HUIB_MIC = 0x54,
    HUIB_PLAYPAUSE = 0x55,
    HUIB_NEXT = 0x57,
    HUIB_PREV = 0x58,
    HUIB_PHONE = 0x5,
    HUIB_START = 126,
    HUIB_STOP = 127,
    HUIB_SCROLLWHEEL = 65536,
    
};

