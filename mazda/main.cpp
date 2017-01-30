#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <linux/input.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <dbus/dbus.h>
#include <poll.h>
#include <inttypes.h>
#include <cmath>
#include <functional>
#include <condition_variable>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include "dbus/generated_cmu.h"

#include "json.hpp"
using json = nlohmann::json;

#include "hu_uti.h"
#include "hu_aap.h"

#include "nm/mzd_nightmode.h"
#include "gps/mzd_gps.h"
#include "bt/mzd_bluetooth.h"

#define EVENT_DEVICE_TS	"/dev/input/filtered-touchscreen0"
#define EVENT_DEVICE_KBD "/dev/input/filtered-keyboard0"

#define HMI_BUS_ADDRESS "unix:path=/tmp/dbus_hmi_socket"
#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

__asm__(".symver realpath1,realpath1@GLIBC_2.11.1");


typedef struct {
	GMainLoop *loop;
} gst_app_t;

static gst_app_t gst_app;

GstElement *mic_pipeline = nullptr;
GstElement *mic_sink = nullptr;

GstElement *aud_pipeline = nullptr;
GstAppSrc *aud_src = nullptr;

GstElement *au1_pipeline = nullptr;
GstAppSrc *au1_src = nullptr;

GstElement *vid_pipeline = nullptr;
GstAppSrc *vid_src = nullptr;
GstElement *vid_sink = nullptr;

#define ASPECT_RATIO_FIX 1

IHUAnyThreadInterface* g_hu = nullptr;


bool display_status = true;
static void set_display_status(bool st)
{
	printf("set_display_status %s\n", st ? "true" : "false");
	if (st != display_status)
	{
        if (vid_sink)
        {
            g_object_set(G_OBJECT(vid_sink), "should-display", st ? TRUE : FALSE, NULL);
            #if ASPECT_RATIO_FIX
            //This gets forgotten for some reason
            g_object_set(G_OBJECT(vid_sink),
                "axis-left", 0,
                "axis-top", -20,
                "disp-width", 800,
                "disp-height", 520,
                NULL);
			#endif
        }
		display_status = st;

 		g_hu->hu_queue_command([st](IHUConnectionThreadInterface& s)
		{
		    HU::VideoFocus videoFocusGained;
		    videoFocusGained.set_mode(st ? HU::VIDEO_FOCUS_MODE_FOCUSED : HU::VIDEO_FOCUS_MODE_UNFOCUSED);
		    videoFocusGained.set_unrequested(true);
		    s.hu_aap_enc_send_message(0, AA_CH_VID, HU_MEDIA_CHANNEL_MESSAGE::VideoFocus, videoFocusGained);
		});

	}
}


static void read_mic_data (GstElement * sink);

static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer *ptr)
{
	gst_app_t *app = (gst_app_t*)ptr;

	switch(GST_MESSAGE_TYPE(message)){

		case GST_MESSAGE_ERROR:{
						   gchar *debug;
						   GError *err;

						   gst_message_parse_error(message, &err, &debug);
						   g_print("Error %s\n", err->message);
						   g_error_free(err);
						   g_free(debug);
						   g_main_loop_quit(app->loop);
					   }
					   break;

		case GST_MESSAGE_WARNING:{
						 gchar *debug;
						 GError *err;
						 gchar *name;

						 gst_message_parse_warning(message, &err, &debug);
						 g_print("Warning %s\nDebug %s\n", err->message, debug);

						 name = (gchar *)GST_MESSAGE_SRC_NAME(message);

						 g_print("Name of src %s\n", name ? name : "nil");
						 g_error_free(err);
						 g_free(debug);
					 }
					 break;

		case GST_MESSAGE_EOS:
					 g_print("End of stream\n");
					 g_main_loop_quit(app->loop);
					 break;

		case GST_MESSAGE_STATE_CHANGED:
					 break;

		default:
//					 g_print("got message %s\n", \
							 gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
					 break;
	}

	return TRUE;
}

static int gst_pipeline_init(gst_app_t *app)
{
	GstBus *bus;
	GstStateChangeReturn state_ret;
	
	GError *error = NULL;
	gst_init(NULL, NULL);

	//if we have ASPECT_RATIO_FIX, cut off the bottom black bar
	const char* vid_pipeline_launch = "appsrc name=mysrc is-live=true block=false max-latency=1000000 do-timestamp=true ! queue ! h264parse ! vpudec low-latency=true framedrop=true framedrop-level-mask=0x200 ! mfw_isink name=mysink "
	#if ASPECT_RATIO_FIX
    "axis-left=0 axis-top=-20 disp-width=800 disp-height=520"
	#else
	"axis-left=0 axis-top=0 disp-width=800 disp-height=480"
	#endif
	" max-lateness=1000000000 sync=false async=false";

	vid_pipeline = gst_parse_launch(vid_pipeline_launch, &error);
		
	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}
	    
	bus = gst_pipeline_get_bus(GST_PIPELINE(vid_pipeline));
	gst_bus_add_watch(bus, (GstBusFunc)bus_callback, app);
	gst_object_unref(bus);

	vid_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysrc"));
    vid_sink = GST_ELEMENT(gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysink"));
	
	gst_app_src_set_stream_type(vid_src, GST_APP_STREAM_TYPE_STREAM);

	aud_pipeline = gst_parse_launch("appsrc name=audsrc is-live=true block=false max-latency=1000000 do-timestamp=true ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=48000, channels=2 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	aud_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (aud_pipeline), "audsrc"));
	
	gst_app_src_set_stream_type((GstAppSrc *)aud_src, GST_APP_STREAM_TYPE_STREAM);


	au1_pipeline = gst_parse_launch("appsrc name=au1src is-live=true block=false max-latency=1000000 do-timestamp=true ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=16000, channels=1 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	au1_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (au1_pipeline), "au1src"));
	
	gst_app_src_set_stream_type((GstAppSrc *)au1_src, GST_APP_STREAM_TYPE_STREAM);



	mic_pipeline = gst_parse_launch("alsasrc name=micsrc ! audioconvert ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, channels=1, rate=16000 ! queue !appsink name=micsink async=false emit-signals=true blocksize=8192",&error);
	
	if (error != NULL) {
		printf("could not construct mic pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}
	
	mic_sink = gst_bin_get_by_name (GST_BIN (mic_pipeline), "micsink");

	g_object_set(G_OBJECT(mic_sink), "throttle-time", 3000000, NULL);
		
	g_signal_connect(mic_sink, "new-buffer", G_CALLBACK(read_mic_data), NULL);
	
	state_ret = gst_element_set_state (mic_pipeline, GST_STATE_READY);

	return 0;

}


uint64_t get_cur_timestamp()
{
	struct timespec tp;
	/* Fetch the time stamp */
	clock_gettime(CLOCK_REALTIME, &tp);

	return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;	
}


static void aa_touch_event(HU::TouchInfo::TOUCH_ACTION action, unsigned int x, unsigned int y) {

	g_hu->hu_queue_command([action, x, y](IHUConnectionThreadInterface& s)
	{
 		HU::InputEvent inputEvent;
	    inputEvent.set_timestamp(get_cur_timestamp());
	    HU::TouchInfo* touchEvent = inputEvent.mutable_touch();
	    touchEvent->set_action(action);
	    HU::TouchInfo::Location* touchLocation = touchEvent->add_location();
	    touchLocation->set_x(x);
	    touchLocation->set_y(y);
	    touchLocation->set_pointer_id(0);

	    /* Send touch event */
	    
	    int ret = s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
	    if (ret < 0) {
	        printf("aa_touch_event(): hu_aap_enc_send() failed with (%d)\n", ret);
	    }
	});
}



static void read_mic_data (GstElement * sink)
{
        
    GstBuffer *gstbuf;
    int ret;
    
    g_signal_emit_by_name (sink, "pull-buffer", &gstbuf,NULL);
    
    if (gstbuf) {

        /* if mic is stopped, don't bother sending */    
        GstState mic_state = GST_STATE_NULL;
        if (gst_element_get_state (mic_pipeline, &mic_state, NULL, GST_CLOCK_TIME_NONE) != GST_STATE_CHANGE_SUCCESS
        	|| mic_state != GST_STATE_PLAYING) {
            printf("Mic stopped.. dropping buffers \n");
            gst_buffer_unref(gstbuf);
            return;
        }
        
           
        gint mic_buf_sz = GST_BUFFER_SIZE (gstbuf);
        
        int idx;
        
        if (mic_buf_sz <= 64) {
            printf("Mic data < 64 \n");
            gst_buffer_unref(gstbuf);
            return;
        }
        
        uint64_t bufTimestamp = GST_BUFFER_TIMESTAMP(gstbuf);
        uint64_t timestamp = GST_CLOCK_TIME_IS_VALID(bufTimestamp) ? bufTimestamp : get_cur_timestamp();
        g_hu->hu_queue_command([timestamp, gstbuf, mic_buf_sz](IHUConnectionThreadInterface& s)
		{
	        int ret = s.hu_aap_enc_send_media_packet(1, AA_CH_MIC, HU_PROTOCOL_MESSAGE::MediaDataWithTimestamp, timestamp, GST_BUFFER_DATA(gstbuf), mic_buf_sz);
	       
	        if (ret < 0) {
	            printf("read_mic_data(): hu_aap_enc_send() failed with (%d)\n", ret);
	        }
	        
	        gst_buffer_unref(gstbuf);
	    });
    }
}


static gboolean delayedShouldDisplayTrue(gpointer data)
{
    set_display_status(true);
    return FALSE;
}

static gboolean delayedShouldDisplayFalse(gpointer data)
{
    set_display_status(false);
    return FALSE;
}

static gboolean delayedToggleShouldDisplay(gpointer data)
{
    set_display_status(!display_status);
    return FALSE;
}

struct TouchScreenState {
	int x;
	int y;
	HU::TouchInfo::TOUCH_ACTION action;
	int action_recvd;
};


static void input_thread_func(int touchfd, int kbdfd, int quitfd) 
{
    TouchScreenState mTouch {0,0,(HU::TouchInfo::TOUCH_ACTION)0,0};
    int maxfdPlus1 = std::max(std::max(touchfd, kbdfd), quitfd) + 1;
    while (true)
    {
        fd_set set;
        int unblocked;

        FD_ZERO(&set);
        FD_SET(touchfd, &set);
        FD_SET(kbdfd, &set);
        FD_SET(quitfd, &set);
        
        unblocked = select(maxfdPlus1, &set, NULL, NULL, NULL);

        if (unblocked == -1)
        {
            printf("Error in read...\n");
            g_main_loop_quit(gst_app.loop);
            break;
        }
        else if (unblocked > 0 && FD_ISSET(quitfd, &set))
        {
            break;
        }
        
        struct input_event events[64];
        const size_t buffer_size = sizeof(events);

        if (FD_ISSET(touchfd, &set))
        {
            ssize_t size = read(touchfd, &events, buffer_size);
            
            if (size == 0 || size == -1)
                break;
            
            if (size < sizeof(input_event)) {
                printf("Error size when reading\n");
                g_main_loop_quit(gst_app.loop);
                break;
            }
            
            int num_chars = size / sizeof(input_event);
            for (int i=0;i < num_chars;i++)
            {
                auto& event = events[i];
                switch (event.type)
                {
                    case EV_ABS:
                        switch (event.code) {
                            case ABS_MT_POSITION_X:
                                mTouch.x = event.value * 800 /4095;
                                break;
                            case ABS_MT_POSITION_Y:
                                #if ASPECT_RATIO_FIX
                                mTouch.y = event.value * 450/4095 + 15;
                                #else
                                mTouch.y = event.value * 480/4095;
                                #endif
                                break;
                        }
                        break;
                    case EV_KEY:
                        if (event.code == BTN_TOUCH) {
                            mTouch.action_recvd = 1;
                            if (event.value == 1) {
                                mTouch.action = HU::TouchInfo::TOUCH_ACTION_PRESS;
                            }
                            else {
                                mTouch.action = HU::TouchInfo::TOUCH_ACTION_RELEASE;
                            }
                        }
                        break;
                    case EV_SYN:
                        if (mTouch.action_recvd == 0) {
                            mTouch.action = HU::TouchInfo::TOUCH_ACTION_DRAG;
                            aa_touch_event(mTouch.action, mTouch.x, mTouch.y);
                        } else {
                            aa_touch_event(mTouch.action, mTouch.x, mTouch.y);
                            mTouch.action_recvd = 0;
                        }
                        break;
                }
            }
        }

        if (FD_ISSET(kbdfd, &set))
        {
            ssize_t size = read(kbdfd, &events, buffer_size);

            if (size == 0 || size == -1)
                break;

            if (size < sizeof(input_event)) {
                printf("Error size when reading\n");
                g_main_loop_quit(gst_app.loop);
                break;
            }

            int num_chars = size / sizeof(input_event);
            for (int i=0;i < num_chars;i++)
            {
                auto& event = events[i];
                if (event.type == EV_KEY && (event.value == 1 || event.value == 0))
                {
                    uint64_t timeStamp = get_cur_timestamp();
                    uint32_t scanCode = 0;
                    int32_t scrollAmount = 0;
                    bool isPressed = (event.value == 1);

                    printf("Key code %i value %i\n", (int)event.code, (int)event.value);
                    switch (event.code)
                    {
                    case KEY_G:
                        printf("KEY_G\n");
                        scanCode = HUIB_MIC;
                        break;
                    //Make the music button play/pause
                    case KEY_E:
                        printf("KEY_E\n");
                        scanCode = HUIB_PLAYPAUSE;
                        break;
                    case KEY_LEFTBRACE:
                        printf("KEY_LEFTBRACE\n");
                        scanCode = HUIB_NEXT;
                        break;
                    case KEY_RIGHTBRACE:
                        printf("KEY_RIGHTBRACE\n");
                        scanCode = HUIB_PREV;
                        break;
                    case KEY_BACKSPACE:
                        printf("KEY_BACKSPACE\n");
                        scanCode = HUIB_BACK;
                        break;
                    case KEY_ENTER:
                        printf("KEY_ENTER\n");
                        scanCode = HUIB_ENTER;
                        break;
                    case KEY_LEFT:
                        printf("KEY_LEFT\n");
                        scanCode = HUIB_LEFT;
                        break;
                    case KEY_N:
                        printf("KEY_N\n");
                        if (isPressed)
                        {
                            scrollAmount = -1;
                        }
                        break;
                    case KEY_RIGHT:
                        printf("KEY_RIGHT\n");
                        scanCode = HUIB_RIGHT;
                        break;
                    case KEY_M:
                        printf("KEY_M\n");
                        if (isPressed)
                        {
                            scrollAmount = 1;
                        }
                        break;
                    case KEY_UP:
                        printf("KEY_UP\n");
                        scanCode = HUIB_UP;
                        break;
                    case KEY_DOWN:
                        printf("KEY_DOWN\n");
                        scanCode = HUIB_DOWN;
                        break;
                    case KEY_HOME:
                        printf("KEY_HOME\n");
                        if (isPressed)
                        {
                            g_main_loop_quit (gst_app.loop);
                        }
                        break;
                    case KEY_R:
                        printf("KEY_R\n");
                        if (isPressed)
                        {
                            g_timeout_add(1, delayedToggleShouldDisplay, NULL);
                        }
                        break;
                    }
                    if (scanCode != 0 || scrollAmount != 0)
                    {
                        g_hu->hu_queue_command([timeStamp, scanCode, scrollAmount, isPressed](IHUConnectionThreadInterface& s)
                        {
                            HU::InputEvent inputEvent;
                            inputEvent.set_timestamp(timeStamp);
                            if (scanCode != 0)
                            {
                                HU::ButtonInfo* buttonInfo = inputEvent.mutable_button()->add_button();
                                buttonInfo->set_is_pressed(isPressed);
                                buttonInfo->set_meta(0);
                                buttonInfo->set_long_press(false);
                                buttonInfo->set_scan_code(scanCode);
                            }
                            if (scrollAmount != 0)
                            {
                                HU::RelativeInputEvent* rel = inputEvent.mutable_rel_event()->mutable_event();
                                rel->set_delta(scrollAmount);
                                rel->set_scan_code(HUIB_SCROLLWHEEL);
                            }
                            s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
                        });
                    }
                }
            }
        } 
	}
}

class BUCPSAClient : public com::jci::bucpsa_proxy,
                     public DBus::ObjectProxy
{
public:
    BUCPSAClient(DBus::Connection &connection)
        : DBus::ObjectProxy(connection, "/com/jci/bucpsa", "com.jci.bucpsa")
    {
    }

    virtual void CommandResponse(const uint32_t& cmdResponse) override {}
    virtual void DisplayMode(const uint32_t& currentDisplayMode) override
    {
        if (currentDisplayMode)
        {
            g_timeout_add(1, delayedShouldDisplayFalse, NULL);
        }
        else
        {
            g_timeout_add(750, delayedShouldDisplayTrue, NULL);
        }
    }
    virtual void ReverseStatusChanged(const int32_t& reverseStatus) override {}
    virtual void PSMInstallStatusChanged(const uint8_t& psmInstalled) override {}
};

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
        TV_TOUCH_SURFACE,
        NATGUI_SURFACE,
        LOOPLOGO_SURFACE,
        TRANLOGOEND_SURFACE,
        TRANLOGO_SURFACE,
        QUICKTRANLOGO_SURFACE,
        EXITLOGO_SURFACE,
        JCI_OPERA_PRIMARY,
        JCI_OPERA_SECONDARY,
        lvdsSurface,
        SCREENREP_IVI_NAME,
        NNG_NAVI_MAP1,
        NNG_NAVI_MAP2,
        NNG_NAVI_HMI,
        NNG_NAVI_TWN,
    };

    void SetRequiredSurfacesByEnum(const std::vector<SURFACES>& surfaces, bool fadeOpera)
    {
        std::ostringstream idString;
        for (size_t i = 0; i < surfaces.size(); i++)
        {
            if (i > 0)
                idString << ",";
            idString << surfaces[i];
        }
        SetRequiredSurfaces(idString.str(), fadeOpera ? 1 : 0);
    }
};


static void nightmode_thread_func(std::condition_variable& quitcv, std::mutex& quitmutex) 
{
	int nightmode = NM_NO_VALUE;
	mzd_nightmode_start();
    while (true)
	{		
		int nightmodenow = mzd_is_night_mode_set();

		// We send nightmode status periodically, otherwise Google Maps
		// doesn't switch to nightmode if it's started late. Even if the
		// other AA UI is already in nightmode.
		if (nightmodenow != NM_NO_VALUE) {
			nightmode = nightmodenow;

			g_hu->hu_queue_command([nightmodenow](IHUConnectionThreadInterface& s)
			{
				HU::SensorEvent sensorEvent;
				sensorEvent.add_night_mode()->set_is_night(nightmodenow);

				s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
			});
		}
		
		{
			std::unique_lock<std::mutex> lk(quitmutex);
    		if (quitcv.wait_for(lk, std::chrono::milliseconds(1000)) == std::cv_status::no_timeout)
    		{
    			break;
    		}
		}
	}

	mzd_nightmode_stop();
}


static void signals_handler (int signum)
{
	if (signum == SIGINT)
	{
		if (gst_app.loop && g_main_loop_is_running (gst_app.loop))
		{
			g_main_loop_quit (gst_app.loop);
		}
	}
}


class MazdaEventCallbacks : public IHUConnectionThreadEventCallbacks
{
public:
  virtual int MediaPacket(int chan, uint64_t timestamp, const byte * buf, int len) override
  {
  	GstAppSrc* gst_src = nullptr;
  	if (chan == AA_CH_VID)
  	{
  		gst_src = vid_src;
  	}
  	else if (chan == AA_CH_AUD)
  	{
  		gst_src  = aud_src;
  	}
  	else if (chan == AA_CH_AU1)
	{
		gst_src = au1_src;
	}

	if (gst_src)
	{
		GstBuffer * buffer = gst_buffer_new_and_alloc(len);
	    memcpy(GST_BUFFER_DATA(buffer), buf, len);
	    int ret = gst_app_src_push_buffer(gst_src, buffer);
	    if(ret !=  GST_FLOW_OK){
	        printf("push buffer returned %d for %d bytes \n", ret, len);
	    }
	}
  	return 0;
  }

  virtual int MediaStart(int chan) override
  {
  	if (chan == AA_CH_MIC)
  	{
		printf("SHAI1 : Mic Started\n");
		gst_element_set_state (mic_pipeline, GST_STATE_PLAYING);
	}	
  	return 0;
  }

  virtual int MediaStop(int chan) override
  {
  	if (chan == AA_CH_MIC)
  	{
		printf("SHAI1 : Mic Stopped\n");
		gst_element_set_state (mic_pipeline, GST_STATE_READY);
	}
  	return 0;
  }

  virtual void DisconnectionOrError() override
  {
  	printf("DisconnectionOrError\n");
  	if (gst_app.loop && g_main_loop_is_running (gst_app.loop))
	{
  		g_main_loop_quit(gst_app.loop);
  	}
  }

  virtual void CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel& streamChannel) override
  {
    #if ASPECT_RATIO_FIX
        if (chan == AA_CH_VID)
        {
            auto videoConfig = streamChannel.mutable_video_configs(0);
            //This adds a 15px border to the top and bottom of the stream, but makes the image 16:9. We chop off the borders and stretch
            //vertically in gstreamer
            videoConfig->set_margin_height(30);
        }
    #endif
  }

  virtual void CustomizeBluetoothService(int chan, HU::ChannelDescriptor::BluetoothService& bluetoothService) {
    bluetoothService.set_car_address(get_bluetooth_mac_address());
  }
};

void gps_location_handler(uint64_t timestamp, double lat, double lng, double bearing, double speed, double alt, double accuracy) {
	logd("[LOC][%" PRIu64 "] - Lat: %f Lng: %f Brng: %f Spd: %f Alt: %f Acc: %f \n", 
			timestamp, lat, lng, bearing, speed, alt, accuracy);

	g_hu->hu_queue_command([timestamp, lat, lng, bearing, speed, alt, accuracy](IHUConnectionThreadInterface& s)
	{
		HU::SensorEvent sensorEvent;
		HU::SensorEvent::LocationData* location = sensorEvent.add_location_data();
		location->set_timestamp(timestamp);
		location->set_latitude(static_cast<int32_t>(lat * 1E7));
		location->set_longitude(static_cast<int32_t>(lng * 1E7));

		if (bearing != 0) {
			location->set_bearing(static_cast<int32_t>(bearing * 1E6));
		}

		// AA expects speed in knots, so convert back
		location->set_speed(static_cast<int32_t>((speed / 1.852) * 1E3));

		if (alt != 0) {
			location->set_altitude(static_cast<int32_t>(alt * 1E2));
		}

		location->set_accuracy(static_cast<int32_t>(accuracy * 1E3));

		s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
	});
}

class AudioManagerClient : public com::xsembedded::ServiceProvider_proxy,
                     public DBus::ObjectProxy
{
    std::map<std::string, int> streamToSessionIds;
    //"USB" as far as the audio manager cares is the normal ALSA sound output
    int usbSessionID = -1;
    int previousSessionID = -1;
    bool waitingForFocusLostEvent = false;

    //These IDs are usually the same, but they depend on the startup order of the services on the car so we can't assume them 100% reliably
    void populateStreamTable()
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
                streamToSessionIds[streamName] = sessionId;

                if (streamName == "USB")
                {
                    usbSessionID = sessionId;
                }
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
public:
    AudioManagerClient(DBus::Connection &connection)
        : DBus::ObjectProxy(connection, "/com/xse/service/AudioManagement/AudioApplication", "com.xsembedded.service.AudioManagement")
    {
        populateStreamTable();
        if (usbSessionID < 0)
        {
            loge("Can't find USB stream. Audio will not work");
        }
    }

    bool canSwitchAudio() { return usbSessionID >= 0; }

    //calling requestAudioFocus directly doesn't work on the audio mgr
    void audioMgrRequestAudioFocus()
    {
        if (previousSessionID >= 0 || waitingForFocusLostEvent)
        {
            //already asked
            return;
        }

        waitingForFocusLostEvent = true;
        previousSessionID = -1;
        json args = { { "sessionId", usbSessionID } };
        std::string result = Request("requestAudioFocus", args.dump());
        printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
    }

    void audioMgrReleaseAudioFocus()
    {
        if (previousSessionID < 0)
        {
            //no focus
            return;
        }

        json args = { { "sessionId", previousSessionID } };
        std::string result = Request("requestAudioFocus", args.dump());
        printf("requestAudioFocus(%s)\n%s\n", args.dump().c_str(), result.c_str());
        previousSessionID = -1;
    }

    virtual void Notify(const std::string& signalName, const std::string& payload) override
    {
        printf("AudioManagerClient::Notify signalName=%s payload=%s\n", signalName.c_str(), payload.c_str());
        if (waitingForFocusLostEvent && signalName == "audioFocusChangeEvent")
        {
            try
            {
                auto result = json::parse(payload);
                std::string streamName = result["streamName"].get<std::string>();
                std::string newFocus = result["newFocus"].get<std::string>();
                if (newFocus == "lost")
                {
                    auto findIt = streamToSessionIds.find(streamName);
                    if (findIt != streamToSessionIds.end())
                    {
                        previousSessionID = findIt->second;
                        printf("Found previous audio sessionId %i for stream %s\n", previousSessionID, streamName.c_str());
                    }
                    else
                    {
                        loge("Can't find previous audio sessionId for stream %s\n", streamName.c_str());
                        previousSessionID = -1;
                    }
                    waitingForFocusLostEvent = false;
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
};

DBus::Glib::BusDispatcher dispatcher;

int main (int argc, char *argv[])
{	
	//Force line-only buffering so we can see the output during hangs
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	hu_log_library_versions();
	hu_install_crash_handler();

    DBus::default_dispatcher = &dispatcher;
    DBus::_init_threading();

    dispatcher.attach(nullptr);
    printf("DBus::Glib::BusDispatcher attached\n");

	//signal (SIGTERM, signals_handler);

	gst_app_t *app = &gst_app;
	int ret = 0;
	errno = 0;
	byte ep_in_addr  = -2;
	byte ep_out_addr = -2;

	/* Init gstreamer pipeline */
	ret = gst_pipeline_init(app);
	if (ret < 0) {
		printf("gst_pipeline_init() ret: %d\n", ret);
		return (-4);
	}


	MazdaEventCallbacks callbacks;
	HUServer headunit(callbacks);

	g_hu = &headunit.GetAnyThreadInterface();

	/* Start AA processing */
	ret = headunit.hu_aap_start (ep_in_addr, ep_out_addr);
	if (ret < 0) {
		printf("Phone is not connected. Connect a supported phone and restart.\n");
		return 0;
	}

	printf("Starting Android Auto...\n");

	/* Open Touchscreen Device */
	int touchfd = open(EVENT_DEVICE_TS, O_RDONLY);

	if (touchfd < 0) {
		fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_TS);
		return -3;
	}

    if (ioctl(touchfd, EVIOCGRAB, 1) < 0)
    {
        fprintf(stderr, "EVIOCGRAB failed on %s\n", EVENT_DEVICE_TS);
        return -3;   
    }

    int kbdfd = open(EVENT_DEVICE_KBD, O_RDONLY);

    if (kbdfd < 0) {
        fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_KBD);
        return -3;
    }

    if (ioctl(kbdfd, EVIOCGRAB, 1) < 0)
    {
        fprintf(stderr, "EVIOCGRAB failed on %s\n", EVENT_DEVICE_TS);
        return -3;   
    }

	int quitpiperw[2];
	if (pipe(quitpiperw) < 0) {
		fprintf(stderr, "Pipe failed");
		return -3;
	}
	int quitp_read = quitpiperw[0];
	int quitp_write = quitpiperw[1];

	std::condition_variable quitcv;
	std::mutex quitmutex;
	
	std::thread input_thread([touchfd, kbdfd, quitp_read](){ input_thread_func(touchfd, kbdfd, quitp_read); } );
	std::thread nm_thread([&quitcv, &quitmutex](){ nightmode_thread_func(quitcv, quitmutex); } );

	/* Start gstreamer pipeline and main loop */

	// GPS processing
	mzd_gps_start(&gps_location_handler);

    gst_element_set_state((GstElement*)vid_pipeline, GST_STATE_PLAYING);
    gst_element_set_state((GstElement*)aud_pipeline, GST_STATE_PLAYING);
    gst_element_set_state((GstElement*)au1_pipeline, GST_STATE_PLAYING);

    gst_app.loop = g_main_loop_new (NULL, FALSE);
    //	g_timeout_add_full(G_PRIORITY_HIGH, 1, myMainLoop, (gpointer)app, NULL);

    printf("Starting Android Auto...\n");
    try
    {
        DBus::Connection hmiBus(HMI_BUS_ADDRESS, false);
        hmiBus.register_bus();

        DBus::Connection serviceBus(SERVICE_BUS_ADDRESS, false);
        serviceBus.register_bus();

        BUCPSAClient bucpsaClient(hmiBus);
        printf("Created BUCPSAClient\n");

        NativeGUICtrlClient guiClient(hmiBus);

        AudioManagerClient audioMgr(serviceBus);
        printf("Created AudioManagerClient\n");

        //By setting these manually we can run even when not launched by the JS code
        guiClient.SetRequiredSurfacesByEnum({NativeGUICtrlClient::TV_TOUCH_SURFACE}, true);

        if (audioMgr.canSwitchAudio())
        {
            audioMgr.audioMgrRequestAudioFocus();
        }

                g_main_loop_run (gst_app.loop);

        if (audioMgr.canSwitchAudio())
        {
            audioMgr.audioMgrReleaseAudioFocus();
        }

        guiClient.SetRequiredSurfacesByEnum({NativeGUICtrlClient::JCI_OPERA_PRIMARY}, true);
    }
    catch(DBus::Error& error)
    {
        loge("DBUS Error: %s: %s", error.name(), error.message());
    }

    gst_element_set_state((GstElement*)vid_pipeline, GST_STATE_NULL);
    gst_element_set_state((GstElement*)aud_pipeline, GST_STATE_NULL);
    gst_element_set_state((GstElement*)au1_pipeline, GST_STATE_NULL);
    gst_element_set_state((GstElement*)mic_pipeline, GST_STATE_NULL);


	printf("quitting...\n");

	//data we write doesn't matter, wake up touch polling thread
	write(quitp_write, &quitp_write, sizeof(quitp_write));

	//wake up night mode polling thread
	quitcv.notify_all();

    printf("waiting for input_thread\n");
	input_thread.join();
    printf("waiting for nm_thread\n");
	nm_thread.join();

	printf("waiting for gps_thread\n");
	mzd_gps_stop();

    printf("shutting down\n");

	close(touchfd);
    close(kbdfd);
    close(quitp_write);
    close(quitp_read);

    g_main_loop_unref(gst_app.loop);
    gst_app.loop = nullptr;

		/* Stop AA processing */
	ret = headunit.hu_aap_shutdown();
	if (ret < 0) {
		printf("hu_aap_shutdown() ret: %d\n", ret);
		ret = -6;
	}

    gst_object_unref(vid_pipeline);
    gst_object_unref(mic_pipeline);
    gst_object_unref(aud_pipeline);
    gst_object_unref(au1_pipeline);

    gst_object_unref(vid_src);
    gst_object_unref(vid_sink);
    gst_object_unref(aud_src);
    gst_object_unref(au1_src);
    gst_object_unref(mic_sink);

	g_hu = nullptr;

	printf("END \n");

	return 0;
}

