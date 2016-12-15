  // Android Auto Protocol Handler
#include <pthread.h>
#define LOGTAG "hu_aap"
#include "hu_uti.h"
#include "hu_ssl.h"
#include "hu_aap.h"
#ifndef NDEBUG
  #include "hu_aad.h"
#endif
#include <fstream>
#include <endian.h>

  int iaap_state = 0; // 0: Initial    1: Startin    2: Started    3: Stoppin    4: Stopped

  const char * chan_get (int chan) {
    switch (chan) {
      case AA_CH_CTR: return ("CTR");
      case AA_CH_VID: return ("VID");
      case AA_CH_TOU: return ("TOU");
      case AA_CH_SEN: return ("SEN");
      case AA_CH_MIC: return ("MIC");
      case AA_CH_AUD: return ("AUD");
      case AA_CH_AU1: return ("AU1");
      case AA_CH_AU2: return ("AU2");
    }
    return ("UNK");
  }
#include "hu_usb.h"
#include "hu_tcp.h"

//dummy functions
  // int hu_tcp_recv  (byte * buf, int len, int tmo) {}                     // Used by hu_aap:hu_aap_tcp_recv ()
  // int hu_tcp_send  (byte * buf, int len, int tmo)  {}                    // Used by hu_aap:hu_aap_tcp_send ()
  // int hu_tcp_stop  () {}                                                 // Used by hu_aap:hu_aap_stop     ()
  // int hu_tcp_start (byte ep_in_addr, byte ep_out_addr) {}                // Used by hu_aap:hu_aap_start    ()


  int transport_type = 1; // 1=USB 2=WiFi
  int ihu_tra_recv  (byte * buf, int len, int tmo) {
    if (transport_type == 1)
      return (hu_usb_recv  (buf, len, tmo));
    else if (transport_type == 2)
      return (hu_tcp_recv  (buf, len, tmo));
    else
      return (-1);
  }
  int ihu_tra_send  (byte * buf, int len, int tmo) {
    if (transport_type == 1)
      return (hu_usb_send  (buf, len, tmo));
    else if (transport_type == 2)
      return (hu_tcp_send  (buf, len, tmo));
    else
      return (-1);
  }
  int ihu_tra_stop  () {
    if (transport_type == 1)
      return (hu_usb_stop  ());
    else if (transport_type == 2)
      return (hu_tcp_stop  ());
    else
      return (-1);
  }


  int iaap_tra_recv_tmo = 150;//100;//1;//10;//100;//250;//100;//250;//100;//25; // 10 doesn't work ? 100 does
  int iaap_tra_send_tmo = 500;//2;//25;//250;//500;//100;//500;//250;

  int ihu_tra_start (byte ep_in_addr, byte ep_out_addr) {
    if (ep_in_addr == 255 && ep_out_addr == 255) {
      logd ("AA over Wifi");
      transport_type = 2;       // WiFi
      iaap_tra_recv_tmo = 1;
      iaap_tra_send_tmo = 2;
    }
    else { 
      transport_type = 1;       // USB
      logd ("AA over USB");
      iaap_tra_recv_tmo = 150;//100;
      iaap_tra_send_tmo = 250;
    }
    if (transport_type == 1)
      return (hu_usb_start  (ep_in_addr, ep_out_addr));
    else if (transport_type == 2)
      return (hu_tcp_start  (ep_in_addr, ep_out_addr));
    else
      return (-1);
  }




  byte enc_buf [DEFBUF] = {0};                                          // Global encrypted transmit data buffer

  byte assy [65536 * 16] = {0};                                         // Global assembly buffer for video fragments: Up to 1 megabyte   ; 128K is fine for now at 800*640
  int assy_size = 0;                                                    // Current size
  int max_assy_size = 0;                                                // Max observed size needed:  151,000

  int vid_rec_ena = 0;                                                // Video recording to file
  int vid_rec_fd  = -1;

  byte vid_ack [] = {0x80, 0x04, 0x08, 0, 0x10,  1};                    // Global Ack: 0, 1

  byte  rx_buf [DEFBUF] = {0};                                          // Global Transport Rx buf
//	byte  *rx_buf;
  //byte dec_buf [DEFBUF] = {0};                                          // Global decrypted receive buffer
  #define dec_buf rx_buf                          // Use same buffer !!!



  int hu_aap_tra_set (int chan, int flags, int type, byte * buf, int len) {  // Convenience function sets up 6 byte Transport header: chan, flags, len, type

    buf [0] = (byte) chan;                                              // Encode channel and flags
    buf [1] = (byte) flags;
    buf [2] = (len -4) / 256;                                            // Encode length of following data:
    buf [3] = (len -4) % 256;
    if (type >= 0) {                                                    // If type not negative, which indicates encrypted type should not be touched...
      buf [4] = type / 256;
      buf [5] = type % 256;                                             // Encode msg_type
    }

    return (len);
  }

  int hu_aap_tra_recv (byte * buf, int len, int tmo) {
    int ret = 0;
    if (iaap_state != hu_STATE_STARTED && iaap_state != hu_STATE_STARTIN) {   // Need to recv when starting
      loge ("CHECK: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
      return (-1);
    }
    ret = ihu_tra_recv (buf, len, tmo);
    if (ret < 0) {
      loge ("ihu_tra_recv() error so stop Transport & AAP  ret: %d", ret);
      hu_aap_stop (); 
    }
    return (ret);
  }

  int log_packet_info = 1;

  int hu_aap_tra_send (int retry, byte * buf, int len, int tmo) {                  // Send Transport data: chan,flags,len,type,...
                                                                        // Need to send when starting
    if (iaap_state != hu_STATE_STARTED && iaap_state != hu_STATE_STARTIN) {
      loge ("CHECK: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
      return (-1);
    }

	
    int ret = ihu_tra_send (buf, len, tmo);
    if (ret < 0 || ret != len) {
      if (retry == 0) {
		hu_aap_stop ();
		loge ("Error ihu_tra_send() error so stop Transport & AAP  ret: %d  len: %d", ret, len);
	  } 
      return (-1);
    }  

    if (ena_log_verbo && ena_log_aap_send)
      logd ("OK ihu_tra_send() ret: %d  len: %d", ret, len);
    return (ret);
  }

  std::vector<uint8_t> tempEncodingBuffer;

  int hu_aap_enc_send_message(int retry, int chan, uint16_t messageCode, const google::protobuf::MessageLite& message)
  {
    const int messageSize = message.ByteSize();
    const int requiredSize = messageSize + 2;
    if (tempEncodingBuffer.size() < requiredSize)
    {
      tempEncodingBuffer.resize(requiredSize);
    }

    uint16_t* destMessageCode = reinterpret_cast<uint16_t*>(tempEncodingBuffer.data());
    *destMessageCode++ = htobe16(messageCode);

    if (!message.SerializeToArray(destMessageCode, messageSize))
    {
      loge("AppendToString failed for %s", message.GetTypeName().c_str());
      return -1; 
    }

    logd ("Send %s on channel %i %s", message.GetTypeName().c_str(), chan, chan_get(chan));
    //hex_dump("PB:", 80, tempEncodingBuffer.data(), requiredSize);
    return hu_aap_enc_send(retry, chan, tempEncodingBuffer.data(), requiredSize);

  }

  int hu_aap_enc_send_media_packet(int retry, int chan, uint16_t messageCode, uint64_t timeStamp, const byte* buffer, int bufferLen)
  {
    const int requiredSize = bufferLen + 2 + 8;
    if (tempEncodingBuffer.size() < requiredSize)
    {
      tempEncodingBuffer.resize(requiredSize);
    }

    uint16_t* destMessageCode = reinterpret_cast<uint16_t*>(tempEncodingBuffer.data());
    *destMessageCode++ = htobe16(messageCode);

    uint64_t* destTimestamp = reinterpret_cast<uint64_t*>(destMessageCode);
    *destTimestamp++ = htobe64(timeStamp);

    memcpy(destTimestamp, buffer, bufferLen);

    //logd ("Send %s on channel %i %s", message.GetTypeName().c_str(), chan, chan_get(chan));
    //hex_dump("PB:", 80, tempEncodingBuffer.data(), requiredSize);
    return hu_aap_enc_send(retry, chan, tempEncodingBuffer.data(), requiredSize);
  }

  int hu_aap_enc_send (int retry,int chan, byte * buf, int len) {                 // Encrypt data and send: type,...
    if (iaap_state != hu_STATE_STARTED) {
      logw ("CHECK: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
      //logw ("chan: %d  len: %d  buf: %p", chan, len, buf);
      //hex_dump (" W/    hu_aap_enc_send: ", 16, buf, len);    // Byebye: hu_aap_enc_send:  00000000 00 0f 08 00
      return (-1);
    }
    int flags = 0x0b;                                                   // Flags = First + Last + Encrypted
    if (chan != AA_CH_CTR && buf [0] == 0) {                            // If not control channel and msg_type = 0 - 255 = control type message
      flags = 0x0f;                                                     // Set Control Flag (On non-control channels, indicates generic/"control type" messages
      //logd ("Setting control");
    }
    if (chan == AA_CH_MIC && buf [0] == 0 && buf [1] == 0) {            // If Mic PCM Data
      flags = 0x0b;                                                     // Flags = First + Last + Encrypted
    }

#ifndef NDEBUG
//    if (ena_log_verbo && ena_log_aap_send) {
    if (log_packet_info) { // && ena_log_aap_send)
      char prefix [DEF_BUF] = {0};
      snprintf (prefix, sizeof (prefix), "S %d %s %1.1x", chan, chan_get (chan), flags);  // "S 1 VID B"
      int rmv = hu_aad_dmp (prefix, "HU", chan, flags, buf, len);
    }
#endif
	

    int bytes_written = SSL_write (hu_ssl_ssl, buf, len);               // Write plaintext to SSL
    if (bytes_written <= 0) {
      loge ("SSL_write() bytes_written: %d", bytes_written);
      hu_ssl_ret_log (bytes_written);
      hu_ssl_inf_log ();
      hu_aap_stop ();
      return (-1);
    }
    if (bytes_written != len)
      loge ("SSL_write() len: %d  bytes_written: %d  chan: %d %s", len, bytes_written, chan, chan_get (chan));
    else if (ena_log_verbo && ena_log_aap_send)
      logd ("SSL_write() len: %d  bytes_written: %d  chan: %d %s", len, bytes_written, chan, chan_get (chan));

    int bytes_read = BIO_read (hu_ssl_wm_bio, & enc_buf [4], sizeof (enc_buf) - 4); // Read encrypted from SSL BIO to enc_buf + 
        
    if (bytes_read <= 0) {
      loge ("BIO_read() bytes_read: %d", bytes_read);
      hu_aap_stop ();
      return (-1);
    }
    if (ena_log_verbo && ena_log_aap_send)
      logd ("BIO_read() bytes_read: %d", bytes_read);

    hu_aap_tra_set (chan, flags, -1, enc_buf, bytes_read + 4);          // -1 for type so encrypted type position is not overwritten !!
    int ret = 0;
    ret = hu_aap_tra_send (retry, enc_buf, bytes_read + 4, iaap_tra_send_tmo);           // Send encrypted data to AA Server
    if (retry)
		  return (ret);

    return (0);
  }

//  extern int wifi_direct;// = 0;//1;//0;
  int hu_handle_ServiceDiscoveryRequest (int chan, byte * buf, int len) {                  // Service Discovery Request

    HU::ServiceDiscoveryRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("Service Discovery Request: %x", buf [2]);
    else
      logd ("Service Discovery Request: %s", request.phone_name().c_str());                               // S 0 CTR b src: HU  lft:   113  msg_type:     6 Service Discovery Response    S 0 CTR b 00000000 0a 08 08 01 12 04 0a 02 08 0b 0a 13 08 02 1a 0f

    HU::ServiceDiscoveryResponse carInfo;
    carInfo.set_head_unit_name("Mazda Connect");
    carInfo.set_car_model("Mazda");
    carInfo.set_car_year("2016");
    carInfo.set_car_serial("0001");
    carInfo.set_driver_pos(true);
    carInfo.set_headunit_make("Mazda");
    carInfo.set_headunit_model("Connect");
    carInfo.set_sw_build("SWB1");
    carInfo.set_sw_version("SWV1");
    carInfo.set_can_play_native_media_during_vr(false);
    carInfo.set_hide_clock(false);

    carInfo.mutable_channels()->Reserve(AA_CH_MAX);
    
    HU::ChannelDescriptor* inputChannel = carInfo.add_channels();
    inputChannel->set_channel_id(AA_CH_TOU);
    {
      auto inner = inputChannel->mutable_input_event_channel();
      auto tsConfig = inner->mutable_touch_screen_config();
      tsConfig->set_width(800);
      tsConfig->set_height(480);

      //No idea what these mean since they aren't the same as HU_INPUT_BUTTON
      //1 seems to be "show cursor"
      inner->add_keycodes_supported(1);
      inner->add_keycodes_supported(2);
      inner->add_keycodes_supported(4);
      inner->add_keycodes_supported(HUIB_BACK);
      inner->add_keycodes_supported(HUIB_UP);
      inner->add_keycodes_supported(HUIB_DOWN);
      inner->add_keycodes_supported(HUIB_LEFT);
      inner->add_keycodes_supported(HUIB_RIGHT);
      inner->add_keycodes_supported(HUIB_ENTER);
      inner->add_keycodes_supported(HUIB_MIC);
      inner->add_keycodes_supported(HUIB_PLAYPAUSE);
      inner->add_keycodes_supported(HUIB_NEXT);
      inner->add_keycodes_supported(HUIB_PREV);
      inner->add_keycodes_supported(HUIB_PHONE);
      inner->add_keycodes_supported(HUIB_SCROLLWHEEL);
      
    }

    HU::ChannelDescriptor* sensorChannel = carInfo.add_channels();
    sensorChannel->set_channel_id(AA_CH_SEN);
    {
      auto inner = sensorChannel->mutable_sensor_channel();
      inner->add_sensor_list()->set_type(HU::SENSOR_TYPE_DRIVING_STATUS);
      inner->add_sensor_list()->set_type(HU::SENSOR_TYPE_NIGHT_DATA);
      inner->add_sensor_list()->set_type(HU::SENSOR_TYPE_LOCATION);
      inner->add_sensor_list()->set_type(HU::SENSOR_TYPE_UNKNOWN_SHOW_KEYBOARD);
    }

    HU::ChannelDescriptor* videoChannel = carInfo.add_channels();
    videoChannel->set_channel_id(AA_CH_VID);
    {
      auto inner = videoChannel->mutable_output_stream_channel();
      inner->set_type(HU::STREAM_TYPE_VIDEO);
      auto videoConfig = inner->add_video_configs();
      videoConfig->set_resolution(HU::ChannelDescriptor::OutputStreamChannel::VideoConfig::VIDEO_RESOLUTION_800x480);
      videoConfig->set_frame_rate(HU::ChannelDescriptor::OutputStreamChannel::VideoConfig::VIDEO_FPS_30);
      videoConfig->set_margin_width(0);
      videoConfig->set_margin_height(0);
      videoConfig->set_dpi(140);
      inner->set_available_while_in_call(false);
    }

    HU::ChannelDescriptor* audioChannel0 = carInfo.add_channels();
    audioChannel0->set_channel_id(AA_CH_AUD);
    {
      auto inner = audioChannel0->mutable_output_stream_channel();
      inner->set_type(HU::STREAM_TYPE_AUDIO);
      inner->set_audio_type(HU::AUDIO_TYPE_MEDIA);
      auto audioConfig = inner->add_audio_configs();
      audioConfig->set_sample_rate(48000);
      audioConfig->set_bit_depth(16);
      audioConfig->set_channel_count(2);
    }

    HU::ChannelDescriptor* audioChannel1 = carInfo.add_channels();
    audioChannel1->set_channel_id(AA_CH_AU1);
    {
      auto inner = audioChannel1->mutable_output_stream_channel();
      inner->set_type(HU::STREAM_TYPE_AUDIO);
      inner->set_audio_type(HU::AUDIO_TYPE_SPEECH);
      auto audioConfig = inner->add_audio_configs();
      audioConfig->set_sample_rate(16000);
      audioConfig->set_bit_depth(16);
      audioConfig->set_channel_count(1);
    }

    HU::ChannelDescriptor* audioChannel2 = carInfo.add_channels();
    audioChannel2->set_channel_id(AA_CH_AU2);
    {
      auto inner = audioChannel1->mutable_output_stream_channel();
      inner->set_type(HU::STREAM_TYPE_AUDIO);
      inner->set_audio_type(HU::AUDIO_TYPE_SYSTEM);
      auto audioConfig = inner->add_audio_configs();
      audioConfig->set_sample_rate(16000);
      audioConfig->set_bit_depth(16);
      audioConfig->set_channel_count(1);
    }

    HU::ChannelDescriptor* micChannel = carInfo.add_channels();
    micChannel->set_channel_id(AA_CH_MIC);
    {
      auto inner = micChannel->mutable_input_stream_channel();
      inner->set_type(HU::STREAM_TYPE_AUDIO);
      auto audioConfig = inner->mutable_audio_config();
      audioConfig->set_sample_rate(16000);
      audioConfig->set_bit_depth(16);
      audioConfig->set_channel_count(1);
    }

    return hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::ServiceDiscoveryResponse, carInfo);
  }

  
  int hu_handle_PingRequest (int chan, byte * buf, int len) {                  // Ping Request
    HU::PingRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("Ping Request");
    else
      logd ("Ping Request: %d", buf[3]);

    HU::PingResponse response;
    response.set_timestamp(request.timestamp());
    return hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::PingResponse, response);
  }

  int hu_handle_NavigationFocusRequest (int chan, byte * buf, int len) {                  // Navigation Focus Request
    HU::NavigationFocusRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("Navigation Focus Request");
    else
      logd ("Navigation Focus Request: %d", request.focus_type());

    HU::NavigationFocusResponse response;
    response.set_focus_type(2); // Gained / Gained Transient ?
    return hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::NavigationFocusResponse, response);
  }

  int hu_handle_ShutdownRequest (int chan, byte * buf, int len) {                  // Byebye Request

    HU::ShutdownRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("Byebye Request");
    else if (request.reason() == 1)
      logd ("Byebye Request reason: 1 AA Exit Car Mode");
    else
      loge ("Byebye Request reason: %d", request.reason());

    HU::ShutdownResponse response;
    hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::ShutdownResponse, response);

    ms_sleep (100);                                                     // Wait a bit for response

    hu_aap_stop ();

    return (-1);
  }
  
  int hu_handle_VoiceSessionRequest (int chan, byte * buf, int len) {                  // sr:  00000000 00 11 08 01      Microphone voice search usage     sr:  00000000 00 11 08 02
    
    HU::VoiceSessionRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("Voice Session Notification");
    else if (request.voice_status() == HU::VoiceSessionRequest::VOICE_STATUS_START)
      logd ("Voice Session Notification: 1 START");
    else if (request.voice_status() == HU::VoiceSessionRequest::VOICE_STATUS_STOP)
      logd ("Voice Session Notification: 2 STOP");
    else
      loge ("Voice Session Notification: %d", request.voice_status());
    return (0);
  }


  int hu_handle_AudioFocusRequest (int chan, byte * buf, int len) {                  // Navigation Focus Request
    HU::AudioFocusRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("AudioFocusRequest Focus Request");
    else
      logd ("AudioFocusRequest Focus Request: %d", request.focus_type());

    HU::AudioFocusResponse response;
    if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_RELEASE)
      response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_LOSS); 
    else if (request.focus_type() == HU::AudioFocusRequest::AUDIO_FOCUS_GAIN_TRANSIENT)
      response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN); 
    else 
      response.set_focus_type(HU::AudioFocusResponse::AUDIO_FOCUS_STATE_GAIN); 

    return hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::AudioFocusResponse, response);
  }

  int hu_handle_ChannelOpenRequest(int chan, byte * buf, int len) {                  // Channel Open Request

    HU::ChannelOpenRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("Channel Open Request");
    else
      logd ("Channel Open Request: %d  priority: %d", request.id(), request.priority());

    HU::ChannelOpenResponse response;
    response.set_status(HU::STATUS_OK);

    int ret = hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::ChannelOpenResponse, response);
    if (ret)                                                            // If error, done with error
      return (ret);

    if (chan == AA_CH_SEN) {                                            // If Sensor channel...
      ms_sleep (2);//20);

      HU::SensorEvent sensorEvent;
      sensorEvent.add_driving_status()->set_is_driving(0);
      return hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
    }
    return (ret);
  }

  int hu_handle_MediaSetupRequest(int chan, byte * buf, int len) {  

    HU::MediaSetupRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("MediaSetupRequest");
    else
      logd ("MediaSetupRequest: %d", request.type());

    HU::MediaSetupResponse response;
    response.set_media_status(HU::MediaSetupResponse::MEDIA_STATUS_2);
    response.set_max_unacked(1);
    response.add_configs(0);

    int ret = hu_aap_enc_send_message(0, chan, HU_MEDIA_CHANNEL_MESSAGE::MediaSetupResponse, response);
    if (ret)                                                            // If error, done with error
      return (ret);

    if (chan == AA_CH_VID) {
      HU::VideoFocus videoFocusGained;
      videoFocusGained.set_mode(HU::VIDEO_FOCUS_MODE_FOCUSED);
      videoFocusGained.set_unrequested(true);
      return hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
    }
    return (ret);
  }


  int hu_handle_VideoFocusRequest(int chan, byte * buf, int len) {  

    HU::VideoFocusRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("VideoFocusRequest");
    else
      logd ("VideoFocusRequest: %d", request.disp_index());

    if (request.mode() == HU::VIDEO_FOCUS_MODE_FOCUSED)
    {
      HU::VideoFocus videoFocusGained;
      videoFocusGained.set_mode(HU::VIDEO_FOCUS_MODE_FOCUSED);
      videoFocusGained.set_unrequested(false);
      return hu_aap_enc_send_message(0, chan, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
    }
    else
    {
      //Tread unfocused as quit (since that's what the "Return to Mazda Connect" button sends)
      HU::VideoFocus videoFocusGained;
      videoFocusGained.set_mode(HU::VIDEO_FOCUS_MODE_UNFOCUSED);
      videoFocusGained.set_unrequested(false);
      hu_aap_enc_send_message(0, chan, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);

      HU::ShutdownRequest request;
      request.set_reason(HU::ShutdownRequest::REASON_QUIT);
      hu_aap_enc_send_message(0, chan, HU_PROTOCOL_MESSAGE::ShutdownRequest, request);

      hu_aap_stop ();
    }
  }

  int32_t channel_session_id[AA_CH_MAX+1] = {0,0,0,0,0,0,0};
  int out_state_channel[AA_CH_MAX+1] = {-1,-1,-1,-1,-1,-1,-1};


  int hu_handle_MediaStartRequest(int chan, byte * buf, int len) {                  // sr:  00000000 00 11 08 01      Microphone voice search usage     sr:  00000000 00 11 08 02
    
    HU::MediaStartRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("MediaStartRequest");
    else
      logd ("MediaStartRequest: %d", request.session());

    channel_session_id[chan] = request.session();
    return (0);
  }

  int hu_aap_out_get (int chan) {
    int state = out_state_channel[chan]; // Get current audio output state change
    out_state_channel[chan] = -1; // Reset audio output state change indication

    return (state);                                                     // Return what the new state was before reset
  }

  int hu_handle_MediaStopRequest(int chan, byte * buf, int len) {                  // sr:  00000000 00 11 08 01      Microphone voice search usage     sr:  00000000 00 11 08 02
    
    HU::MediaStopRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("MediaStopRequest");
    else
      logd ("MediaStopRequest");

    out_state_channel[chan] = 1;
    return (0);
  }


  int hu_handle_SensorStartRequest (int chan, byte * buf, int len) {                  // Navigation Focus Request
    HU::SensorStartRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("SensorStartRequest Focus Request");
    else
      logd ("SensorStartRequest Focus Request: %d", request.type());

    HU::SensorStartResponse response;
    response.set_status(HU::STATUS_OK);

    return hu_aap_enc_send_message(0, chan, HU_SENSOR_CHANNEL_MESSAGE::SensorStartResponse, response);
  }
  
  
  int hu_handle_BindingRequest (int chan, byte * buf, int len) {                  // Navigation Focus Request
    HU::BindingRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("BindingRequest Focus Request");
    else
      logd ("BindingRequest Focus Request: %d", request.scan_codes_size());

    HU::BindingResponse response;
    response.set_status(HU::STATUS_OK);

    return hu_aap_enc_send_message(0, chan, HU_INPUT_CHANNEL_MESSAGE::BindingResponse, response);
  }

  int mic_change_status = 0;
  int hu_aap_mic_get () {
    int ret_status = mic_change_status;                                 // Get current mic change status
    if (mic_change_status == 2 || mic_change_status == 1) {             // If start or stop...
      mic_change_status = 0;                                            // Reset mic change status to "No Change"
    }
    return (ret_status);                                                // Return original mic change status
  }

  int hu_handle_MediaAck (int chan, byte * buf, int len) {

    HU::MediaAck request;
    if (!request.ParseFromArray(buf, len))
      loge ("MediaAck");
    else
      logd ("MediaAck");
    return (0);
  }

  int hu_handle_MicRequest (int chan, byte * buf, int len) {

    HU::MicRequest request;
    if (!request.ParseFromArray(buf, len))
      loge ("MicRequest");
    else
      logd ("MicRequest");

    if (!request.open()) {
      logd ("Mic Start/Stop Request: 0 STOP");
      mic_change_status = 1;                                            // Stop Mic
    }
    else {
      logd ("Mic Start/Stop Request: 1 START");
      mic_change_status = 2;                                            // Start Mic
    }
    return (0);
  }



  void iaap_video_decode (byte * buf, int len) {
    hu_write_to_buffer(AA_CH_VID, buf, len);
  }

/* 8,192 bytes per packet at stereo 48K 16 bit = 42.667 ms per packet                            Timestamp = uptime in microseconds:
ms: 337, 314                                                                                  0x71fd616538  0x71fd620560   0x71fd62a970 (489,582,406,000)
                                                                                           diff:  0xA028 (41000)    0xA410  (42000)
07-01 18:54:11.067 W/                        hex_dump(28628): AUDIO:  00000000 00 00 00 00 00 71 fd 61 65 38 00 00 00 00 00 00 
07-01 18:54:11.404 W/                        hex_dump(28628): AUDIO:  00000000 00 00 00 00 00 71 fd 62 05 60 00 00 00 00 00 00 
07-01 18:54:11.718 W/                        hex_dump(28628): AUDIO:  00000000 00 00 00 00 00 71 fd 62 a9 70 00 00 00 00 00 00 

*/

  int aud_rec_ena = 0;                                                // Audio recording to file
  int aud_rec_fd  = -1;

  void iaap_audio_decode (int chan, byte * buf, int len) {
    #define aud_buf_BUFS_SIZE    65536 * 4      // Up to 256 Kbytes
    if (len > aud_buf_BUFS_SIZE) {
      loge ("Error audio len: %d  aud_buf_BUFS_SIZE: %d", len, aud_buf_BUFS_SIZE);
      len = aud_buf_BUFS_SIZE;
    }

    hu_write_to_buffer(chan, buf, len);
  }



  byte aud_ack [] = {0x80, 0x04, 0x08, 0, 0x10,  1};                    // Global Ack: 0, 1     Same as video ack ?

  //int aud_ack_ctr = 0;
  int iaap_audio_process (int chan, int msg_type, int flags, byte * buf, int len) { // 300 ms @ 48000/sec   samples = 14400     stereo 16 bit results in bytes = 57600
    //loge ("????????????????????? !!!!!!!!!!!!!!!!!!!!!!!!!   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!   aud_ack_ctr: %d  len: %d", aud_ack_ctr ++, len);

    //logd ("iaap_audio_process chan: %d  msg_type: %d  flags: 0x%x  buf: %p  len: %d", chan, msg_type, flags, buf, len); // iaap_audio_process msg_type: 0  flags: 0xb  buf: 0xe08cbfb8  len: 8202

    HU::MediaAck audioAck;
    audioAck.set_session(channel_session_id[chan]);
    audioAck.set_value(1);

    hu_aap_enc_send_message(0, chan, HU_MEDIA_CHANNEL_MESSAGE::MediaAck, audioAck);
    if (len >= 10) {
/*
07-02 03:33:26.486 W/                        hex_dump( 1549): AUDIO:  00000000 00 00 00 00 00 79 3e 5c bd 60 45 ef 6c 1a 79 f6 
07-02 03:33:26.486 W/                        hex_dump( 1549): AUDIO:      0010 a8 15 15 fe b3 14 8c fc e8 0c 34 f8 bf 02 ec 00 
07-02 03:33:26.486 W/                        hex_dump( 1549): AUDIO:      0020 ab 0a 9a 0d a1 1d 88 0a ae 1e e5 03 a9 16 8d 10 
07-02 03:33:26.486 W/                        hex_dump( 1549): AUDIO:      0030 d9 1f 3c 28 af 34 9b 35 e2 3e e2 36 fd 3c b4 34 
07-02 03:33:26.487 D/              iaap_audio_process( 1549): iaap_audio_process ts: 31038 0x793e  t2: 31038 0x793e
07-02 03:33:26.487 D/              iaap_audio_process( 1549): iaap_audio_process ts: 1046265184 0x3e5cbd60  t2: 1046265184 0x3e5cbd60
*/
      iaap_audio_decode (chan, & buf [10], len - 10);//assy, assy_size);                                                                                    // Decode PCM audio fully re-assembled
    }

    return (0);
  }

  //int vid_ack_ctr = 0;
  int iaap_video_process (int msg_type, int flags, byte * buf, int len) {    // Process video packet
// MaxUnack
//loge ("????????????????????? !!!!!!!!!!!!!!!!!!!!!!!!!   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!   vid_ack_ctr: %d  len: %d", vid_ack_ctr ++, len);


    HU::MediaAck videoAck;
    videoAck.set_session(channel_session_id[AA_CH_VID]);
    videoAck.set_value(1);

    hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::MediaAck, videoAck);

    if (0) {
    }
    else if (flags == 11 && (msg_type == 0 || msg_type == 1) && (buf [10] == 0 && buf [11] == 0 && buf [12] == 0 && buf [13] == 1)) {  // If Not fragmented Video
      iaap_video_decode (& buf [10], len - 10);                                                                               // Decode H264 video
    }
    else if (flags == 9 && (msg_type == 0 || msg_type == 1) && (buf [10] == 0 && buf [11] == 0 && buf [12] == 0 && buf [13] == 1)) {   // If First fragment Video
      memcpy (assy, & buf [10], len - 10);                                                                                    // Len in bytes 2,3 doesn't include total len 4 bytes at 4,5,6,7
      assy_size = len - 10;                                                                                                   // Add to re-assembly in progress
    }
    else if (flags == 11 && msg_type == 1 && (buf [2] == 0 && buf [3] == 0 && buf [4] == 0 && buf [5] == 1)) {                     // If Not fragmented First video config packet
      iaap_video_decode (& buf [2], len - 2);                                                                                 // Decode H264 video
    }
    else if (flags == 8) {                                                                                                     // If Middle fragment Video
      memcpy (& assy [assy_size], buf, len);
      assy_size += len;                                                                                                       // Add to re-assembly in progress
    }
    else if (flags == 10) {                                                                                                    // If Last fragment Video
      memcpy (& assy [assy_size], buf, len);
      assy_size += len;                                                                                                       // Add to re-assembly in progress
      iaap_video_decode (assy, assy_size);                                                                                    // Decode H264 video fully re-assembled
    }
    else
      loge ("Video error msg_type: %d  flags: 0x%x  buf: %p  len: %d", msg_type, flags, buf, len);

    return (0);
  }

  int iaap_msg_process (int chan, int flags, byte * buf, int len) {

    uint16_t msg_type = be16toh(*reinterpret_cast<uint16_t*>(buf));

    if (ena_log_verbo)
      logd ("iaap_msg_process msg_type: %d  len: %d  buf: %p", msg_type, len, buf);

    int run = 0;
    if ((chan == AA_CH_AUD || chan == AA_CH_AU1 || chan == AA_CH_AU2) && ((HU_PROTOCOL_MESSAGE)msg_type == HU_PROTOCOL_MESSAGE::MediaData0 || (HU_PROTOCOL_MESSAGE)msg_type == HU_PROTOCOL_MESSAGE::MediaData1)) {// || flags == 8 || flags == 9 || flags == 10 ) {         // If Audio Output...
      return (iaap_audio_process (chan, msg_type, flags, buf, len)); // 300 ms @ 48000/sec   samples = 14400     stereo 16 bit results in bytes = 57600
    }
    else if (chan == AA_CH_VID && ((HU_PROTOCOL_MESSAGE)msg_type == HU_PROTOCOL_MESSAGE::MediaData0 || (HU_PROTOCOL_MESSAGE)msg_type == HU_PROTOCOL_MESSAGE::MediaData1 || flags == 8 || flags == 9 || flags == 10)) {    // If Video...
      return (iaap_video_process (msg_type, flags, buf, len));
    }

    //remove the message type
    buf += 2;
    len -= 2;

    const bool isControlMessage = msg_type < 0x8000;

    if (isControlMessage)
    {
      switch((HU_PROTOCOL_MESSAGE)msg_type)
      {
        case HU_PROTOCOL_MESSAGE::ServiceDiscoveryRequest:
          return hu_handle_ServiceDiscoveryRequest(chan, buf, len);
        case HU_PROTOCOL_MESSAGE::ChannelOpenRequest:
          return hu_handle_ChannelOpenRequest(chan, buf, len);
        case HU_PROTOCOL_MESSAGE::PingRequest:
          return hu_handle_PingRequest(chan, buf, len);
        case HU_PROTOCOL_MESSAGE::NavigationFocusRequest:
          return hu_handle_NavigationFocusRequest(chan, buf, len);
        case HU_PROTOCOL_MESSAGE::ShutdownRequest:
          return hu_handle_ShutdownRequest(chan, buf, len);
        case HU_PROTOCOL_MESSAGE::VoiceSessionRequest:
          return hu_handle_VoiceSessionRequest(chan, buf, len);
        case HU_PROTOCOL_MESSAGE::AudioFocusRequest:
          return hu_handle_AudioFocusRequest(chan, buf, len);
        default:
          loge ("Unknown msg_type: %d", msg_type);
          return (0);
      }
    }
    else if (chan == AA_CH_SEN)
    {
      switch((HU_SENSOR_CHANNEL_MESSAGE)msg_type)
      {
        case HU_SENSOR_CHANNEL_MESSAGE::SensorStartRequest:
          return hu_handle_SensorStartRequest(chan, buf, len);
        default:
          loge ("Unknown msg_type: %d", msg_type);
          return (0);
      }
    }
    else if (chan == AA_CH_TOU)
    {
      switch((HU_INPUT_CHANNEL_MESSAGE)msg_type)
      {
        case HU_INPUT_CHANNEL_MESSAGE::BindingRequest:
          return hu_handle_BindingRequest(chan, buf, len);
        default:
          loge ("Unknown msg_type: %d", msg_type);
          return (0);
      }
    }
    else if (chan == AA_CH_AUD || chan == AA_CH_AU1 || chan == AA_CH_AU2 || chan == AA_CH_VID || chan == AA_CH_MIC)
    {
      switch((HU_MEDIA_CHANNEL_MESSAGE)msg_type)
      {
        case HU_MEDIA_CHANNEL_MESSAGE::MediaSetupRequest:
          return hu_handle_MediaSetupRequest(chan, buf, len);
        case HU_MEDIA_CHANNEL_MESSAGE::MediaStartRequest:
          return hu_handle_MediaStartRequest(chan, buf, len);
        case HU_MEDIA_CHANNEL_MESSAGE::MediaStopRequest:
          return hu_handle_MediaStopRequest(chan, buf, len);
        case HU_MEDIA_CHANNEL_MESSAGE::MediaAck:
          return hu_handle_MediaAck(chan, buf, len);
        case HU_MEDIA_CHANNEL_MESSAGE::MicRequest:
          return hu_handle_MicRequest(chan, buf, len);
        case HU_MEDIA_CHANNEL_MESSAGE::VideoFocusRequest:
          return hu_handle_VideoFocusRequest(chan, buf, len);
        default:
          loge ("Unknown msg_type: %d", msg_type);
          return (0);
      }
    }

    loge ("Unknown chan: %d", chan);
    return (0);
  }

  int hu_aap_stop () {                                                  // Sends Byebye, then stops Transport/USBACC/OAP

                                                                        // Continue only if started or starting...
    if (iaap_state != hu_STATE_STARTED && iaap_state != hu_STATE_STARTIN)
      return (0);

    // Send Byebye
    iaap_state = hu_STATE_STOPPIN;
    logd ("  SET: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));

    int ret = ihu_tra_stop ();                                           // Stop Transport/USBACC/OAP
    iaap_state = hu_STATE_STOPPED;
    logd ("  SET: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
    
//    g_free(rx_buf);
//	thread_cleanup();
    return (ret);
  }

  int hu_aap_start (byte ep_in_addr, byte ep_out_addr) {                // Starts Transport/USBACC/OAP, then AA protocol w/ VersReq(1), SSL handshake, Auth Complete

//	rx_buf = (byte *)g_malloc(DEFBUF);
	
    if (iaap_state == hu_STATE_STARTED) {
      loge ("CHECK: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
      return (0);
    }

    iaap_state = hu_STATE_STARTIN;
    logd ("  SET: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));

    int ret = ihu_tra_start (ep_in_addr, ep_out_addr);                   // Start Transport/USBACC/OAP
    if (ret) {
      iaap_state = hu_STATE_STOPPED;
      logd ("  SET: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
      return (ret);                                                     // Done if error
    }

    byte vr_buf [] = {0, 3, 0, 6, 0, 1, 0, 1, 0, 1};                    // Version Request
    ret = hu_aap_tra_set (0, 3, 1, vr_buf, sizeof (vr_buf));
    ret = hu_aap_tra_send (0, vr_buf, sizeof (vr_buf), 1000);              // Send Version Request
    if (ret < 0) {
      loge ("Version request send ret: %d", ret);
      hu_aap_stop ();
      return (-1);
    }  

//    byte buf [DEFBUF] = {0};
	byte *buf = (byte *)malloc(DEFBUF);
    errno = 0;
    ret = hu_aap_tra_recv (buf, DEFBUF, 1000);                    // Get Rx packet from Transport:    Wait for Version Response
    if (ret <= 0) {
      loge ("Version response recv ret: %d", ret);
      free(buf);
      hu_aap_stop ();
      return (-1);
    }  
    logd ("Version response recv ret: %d", ret);


	free(buf);
//*
    ret = hu_ssl_handshake ();                                          // Do SSL Client Handshake with AA SSL server
    if (ret) {
      hu_aap_stop ();
      return (ret);
    }

    byte ac_buf [] = {0, 3, 0, 4, 0, 4, 8, 0};                          // Status = OK
    ret = hu_aap_tra_set (0, 3, 4, ac_buf, sizeof (ac_buf));
    ret = hu_aap_tra_send (0, ac_buf, sizeof (ac_buf), 1000);              // Auth Complete, must be sent in plaintext
    if (ret < 0) {
      loge ("hu_aap_tra_send() ret: %d", ret);
      hu_aap_stop ();
      return (-1);
    }  
    hu_ssl_inf_log ();

    iaap_state = hu_STATE_STARTED;
    logd ("  SET: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
//*/
    return (0);
  }



/*
http://stackoverflow.com/questions/22753221/openssl-read-write-handshake-data-with-memory-bio
http://www.roxlu.com/2014/042/using-openssl-with-memory-bios
https://www.openssl.org/docs/ssl/SSL_read.html
http://blog.davidwolinsky.com/2009/10/memory-bios-and-openssl.html
http://www.cisco.com/c/en/us/support/docs/security-vpn/secure-socket-layer-ssl/116181-technote-product-00.html
*/


  int iaap_recv_dec_process (int chan, int flags, byte * buf, int len) {// Decrypt & Process 1 received encrypted message
	
    int bytes_written = BIO_write (hu_ssl_rm_bio, buf, len);           // Write encrypted to SSL input BIO
    
    
    if (bytes_written <= 0) {
      loge ("BIO_write() bytes_written: %d", bytes_written);
      return (-1);
    }
    if (bytes_written != len)
      loge ("BIO_write() len: %d  bytes_written: %d  chan: %d %s", len, bytes_written, chan, chan_get (chan));
    else if (ena_log_verbo)
      logd ("BIO_write() len: %d  bytes_written: %d  chan: %d %s", len, bytes_written, chan, chan_get (chan));

    errno = 0;
    int ctr = 0;
    int max_tries = 2;  // Higher never works
    int bytes_read = -1;
    while (bytes_read <= 0 && ctr ++ < max_tries) {
      bytes_read = SSL_read (hu_ssl_ssl, dec_buf, sizeof (dec_buf));   // Read decrypted to decrypted rx buf
      if (bytes_read <= 0) {
        loge ("ctr: %d  SSL_read() bytes_read: %d  errno: %d", ctr, bytes_read, errno);
        hu_ssl_ret_log (bytes_read);
        ms_sleep (1);
      }
      //logd ("ctr: %d  SSL_read() bytes_read: %d  errno: %d", ctr, bytes_read, errno);
    }

    if (bytes_read <= 0) {
      loge ("ctr: %d  SSL_read() bytes_read: %d  errno: %d", ctr, bytes_read, errno);
      hu_ssl_ret_log (bytes_read);
      return (-1);                                                      // Fatal so return error and de-initialize; Should we be able to recover, if Transport data got corrupted ??
    }
    if (ena_log_verbo)
      logd ("ctr: %d  SSL_read() bytes_read: %d", ctr, bytes_read);

#ifndef NDEBUG
////    if (chan != AA_CH_VID)                                          // If not video...
      if (log_packet_info) {
        char prefix [DEF_BUF] = {0};
        snprintf (prefix, sizeof (prefix), "R %d %s %1.1x", chan, chan_get (chan), flags);  // "R 1 VID B"
        int rmv = hu_aad_dmp (prefix, "AA", chan, flags, dec_buf, bytes_read);           // Dump decrypted AA
      }
#endif

    int prot_func_ret = iaap_msg_process (chan, flags, dec_buf, bytes_read);      // Process decrypted AA protocol message
    return (0);//prot_func_ret);
  }


    //  Process 1 encrypted "receive message set":
      // - Read encrypted message from Transport
      // - Process/react to decrypted message by sending responses etc.
/*
        Tricky issues:

          - Read() may return less than a full packet.
              USB is somewhat "packet oriented" once I raised DEFBUF/sizeof(rx_buf) from 16K to 64K (Maximum video fragment size)
              But TCP is more stream oriented.

          - Read() may contain multiple packets, returning all or the end of one packet, plus all or the beginning of the next packet.
              So far I have only seen 2 complete packets in one read().

          - Read() may return all or part of video stream data fragments. Multiple fragments need to be re-assembled before H.264 video processing.
            Fragments may be up to 64K - 256 in size. Maximum re-assembled video packet seen is around 150K; using 256K re-assembly buffer at present.
            

*/

  int hu_aap_recv_process () {                                          // 
                                                                        // Terminate unless started or starting (we need to process when starting)
    if (iaap_state != hu_STATE_STARTED && iaap_state != hu_STATE_STARTIN) {
      loge ("CHECK: iaap_state: %d (%s)", iaap_state, state_get (iaap_state));
      return (-1);
    }

    byte * buf = rx_buf;
    int ret = 0;
    errno = 0;
    int min_size_hdr = 6;
    int rx_len = sizeof (rx_buf);
    if (transport_type == 2)                                            // If wifi...
      rx_len = min_size_hdr;                                            // Just get the header

    int have_len = 0;                                                   // Length remaining to process for all sub-packets plus 4/8 byte headers

    have_len = hu_aap_tra_recv (rx_buf, rx_len, iaap_tra_recv_tmo);     // Get Rx packet from Transport

    if (have_len == 0) {                                                // If no data, then done w/ no data
      return (0);
    }
    if (have_len < min_size_hdr) {                                      // If we don't have a full 6 byte header at least...
      loge ("Recv have_len: %d", have_len);
      hu_aap_stop ();
      return (-1);
    }  

    while (have_len > 0) {                                              // While length remaining to process,... Process Rx packet:
      if (ena_log_verbo) {
        logd ("Recv while (have_len > 0): %d", have_len);
        hex_dump ("LR: ", 16, buf, have_len);
      }
      int chan = (int) buf [0];                                         // Channel
      int flags = buf [1];                                              // Flags

      int enc_len = (int) buf [3];                                      // Encoded length of bytes to be decrypted (minus 4/8 byte headers)
      enc_len += ((int) buf [2] * 256);

      int msg_type = (int) buf [5];                                     // Message Type (or post handshake, mostly indicator of SSL encrypted data)
      msg_type += ((int) buf [4] * 256);

      have_len -= 4;                                                    // Length starting at byte 4: Unencrypted Message Type or Encrypted data start
      buf += 4;                                                         // buf points to data to be decrypted
      if (flags & 0x08 != 0x08) {
        loge ("NOT ENCRYPTED !!!!!!!!! have_len: %d  enc_len: %d  buf: %p  chan: %d %s  flags: 0x%x  msg_type: %d", have_len, enc_len, buf, chan, chan_get (chan), flags, msg_type);
        hu_aap_stop ();
        return (-1);
      }
      if (chan == AA_CH_VID && flags == 9) {                            // If First fragment Video... (Packet is encrypted so we can't get the real msg_type or check for 0, 0, 0, 1)
        int total_size = (int) buf [3];
        total_size += ((int) buf [2] * 256);
        total_size += ((int) buf [1] * 256 * 256);
        total_size += ((int) buf [0] * 256 * 256 * 256);

        if (total_size > max_assy_size)                                 // If new  max_assy_size... (total_size seen as big as 151 Kbytes)
          max_assy_size = total_size;                                   // Set new max_assy_size      See: jni/hu_aap.c:  byte assy [65536 * 16] = {0}; // up to 1 megabyte
                                                                         //                               & jni/hu_uti.c:  #define vid_buf_BUFS_SIZE    65536 * 4
                                                                        // Up to 256 Kbytes// & src/ca/yyx/hu/hu_tro.java:    byte [] assy = new byte [65536 * 16];
                                                                        // & src/ca/yyx/hu/hu_tra.java:      res_buf = new byte [65536 * 4];
        if (total_size > 160 * 1024)
          logw ("First fragment total_size: %d  max_assy_size: %d", total_size, max_assy_size);
        else
          logv ("First fragment total_size: %d  max_assy_size: %d", total_size, max_assy_size);
        have_len -= 4;                                                  // Remove 4 length bytes inserted into first video fragment
        buf += 4;
      }
      if (have_len < enc_len) {                                         // If we need more data for the full packet...
        int need_len = enc_len - have_len;
        if (transport_type != 2 || rx_len != min_size_hdr)              // If NOT wifi...
          logd ("have_len: %d < enc_len: %d  need_len: %d", have_len, enc_len, need_len);

        // Move the buffer back to the start
        memmove(rx_buf, buf, have_len);
        buf = rx_buf;

        int need_ret = hu_aap_tra_recv (& buf [have_len], need_len, -1);// Get Rx packet from Transport. Use -1 instead of iaap_tra_recv_tmo to indicate need to get need_len bytes
                                                                        // Length remaining for all sub-packets plus 4/8 byte headers
        if (need_ret != need_len) {                                     // If we didn't get precisely the number of bytes we need...
          loge ("Recv need_ret: %d", need_ret);
          hu_aap_stop ();
          return (-1);
        }
        have_len = enc_len;                                             // Length to process now = encoded length for 1 packet
      }

      /*logd ("Calling iaap_recv_dec_process() with have_len: %d  enc_len: %d  buf: %p  chan: %d %s  flags: 0x%x  msg_type: %d", have_len, enc_len, buf, chan, chan_get (chan), flags, msg_type);
      byte sum = 0;
      int ctr = 0;     
      for (ctr = 0; ctr < enc_len; ctr ++)
        sum += buf [ctr];
      logd ("iaap_recv_dec_process() sum: %d", sum);*/
      ret = iaap_recv_dec_process (chan, flags, buf, enc_len);          // Decrypt & Process 1 received encrypted message
      if (ret < 0) {                                                    // If error...
        loge ("Error iaap_recv_dec_process() ret: %d  have_len: %d  enc_len: %d  buf: %p  chan: %d %s  flags: 0x%x  msg_type: %d", ret, have_len, enc_len, buf, chan, chan_get (chan), flags, msg_type);
        hu_aap_stop ();
        return (ret);  
      }
/*
      if (log_packet_info) {
        if (chan == AA_CH_VID && (flags == 8 || flags == 0x0a || msg_type == 0)) // || msg_type ==1))
          ;
        //else if (chan == AA_CH_VID && msg_type == 32768 + 4)
        //  ;
        else {
          logd ("        OK iaap_recv_dec_process() ret: %d  have_len: %d  enc_len: %d  buf: %p  chan: %d %s  flags: 0x%x  msg_type: %d", ret, have_len, enc_len, buf, chan, chan_get (chan), flags, msg_type);
          //logd ("--------------------------------------------------------");  // Empty line / 56 characters
        }
      }
*/
      have_len -= enc_len;                                              // Consume processed sub-packet and advance to next, if any
      buf += enc_len;
      if (have_len != 0)
        logd ("iaap_recv_dec_process() more than one message   have_len: %d  enc_len: %d", have_len, enc_len);
    }

    return (ret);                                                       // Return value from the last iaap_recv_dec_process() call; should be 0
  }
/*
*/
