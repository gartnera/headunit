#include "outputs.h"
#include "main.h"
#include "callbacks.h"

#include "json/json.hpp"
using json = nlohmann::json;

#include <linux/input.h>
#include <linux/uinput.h>

#define EVENT_DEVICE_TS	"/dev/input/filtered-touchscreen0"
#define EVENT_DEVICE_KBD "/dev/input/filtered-keyboard0"
#define EVENT_DEVICE_UI "/dev/uinput"

static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer *ptr)
{
    gst_app_t *app = &gst_app;

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

struct TouchScreenState {
    int x;
    int y;
    HU::TouchInfo::TOUCH_ACTION action;
    int action_recvd;
};

static void aa_touch_event(HU::TouchInfo::TOUCH_ACTION action, unsigned int x, unsigned int y, uint64_t ts) {

    g_hu->hu_queue_command([action, x, y, ts](IHUConnectionThreadInterface& s)
    {
        HU::InputEvent inputEvent;
        inputEvent.set_timestamp(ts);
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

static uint64_t get_timestamp(struct input_event& ii)
{
    return ii.time.tv_sec * 1000000 + ii.time.tv_usec;
}
static void emit(int fd, int type, int code, int val)
{
  struct input_event ie;
  ie.type = type;
  ie.code = code;
  ie.value = val;
  ie.time.tv_sec = 0;
  ie.time.tv_usec = 0;
  write(fd, &ie, sizeof(ie));
}
/**
* Passes the keystroke to MZD by "ungrabbing" the kbd on key-down, simulating the same keystroke with uinput,
* then "re-grabbing" the kbd on key-up.
*/
void VideoOutput::pass_key_to_mzd(int type, int code, int val)
{
  if (val && ioctl(kbd_fd, EVIOCGRAB, 0) < 0)
  {
    fprintf(stderr, "EVIOCGRAB failed to release %s\n", EVENT_DEVICE_KBD);
  }

  emit(ui_fd, type, code, val);
  emit(ui_fd, EV_SYN, SYN_REPORT, 0);

  if(!val && ioctl(kbd_fd, EVIOCGRAB, 1) < 0)
  {
    fprintf(stderr, "EVIOCGRAB failed to grab %s\n", EVENT_DEVICE_KBD);
  }
}
void VideoOutput::input_thread_func()
{
    TouchScreenState mTouch {0,0,(HU::TouchInfo::TOUCH_ACTION)0,0};
    int maxfdPlus1 = std::max(std::max(touch_fd, kbd_fd), input_thread_quit_pipe_read) + 1;
    while (true)
    {
        fd_set set;
        int unblocked;

        FD_ZERO(&set);
        FD_SET(touch_fd, &set);
        FD_SET(kbd_fd, &set);
        FD_SET(input_thread_quit_pipe_read, &set);

        unblocked = select(maxfdPlus1, &set, NULL, NULL, NULL);

        if (unblocked == -1)
        {
            printf("Error in read...\n");
            g_main_loop_quit(gst_app.loop);
            break;
        }
        else if (unblocked > 0 && FD_ISSET(input_thread_quit_pipe_read, &set))
        {
            break;
        }

        struct input_event events[64];
        const size_t buffer_size = sizeof(events);

        if (FD_ISSET(touch_fd, &set))
        {
            ssize_t size = read(touch_fd, &events, buffer_size);

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
                            aa_touch_event(mTouch.action, mTouch.x, mTouch.y, get_timestamp(event));
                        } else {
                            aa_touch_event(mTouch.action, mTouch.x, mTouch.y, get_timestamp(event));
                            mTouch.action_recvd = 0;
                        }
                        break;
                }
            }
        }

        if (FD_ISSET(kbd_fd, &set))
        {
            ssize_t size = read(kbd_fd, &events, buffer_size);

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
                    uint64_t timeStamp = get_timestamp(event);
                    uint32_t scanCode = 0;
                    int32_t scrollAmount = 0;
                    bool isPressed = (event.value == 1);
                    bool longPress = false;
                    AudioManagerClient::FocusType audioFocus = callbacks->audioFocus;
                    bool hasMediaAudioFocus = audioFocus == AudioManagerClient::FocusType::PERMANENT;
                    bool hasAudioFocus = audioFocus != AudioManagerClient::FocusType::NONE;

                    //printf("Key code %i value %i\n", (int)event.code, (int)event.value);
                    switch (event.code)
                    {
                    case KEY_G:
                        printf("KEY_G\n");
                        scanCode = HUIB_MIC;
                        break;
                    //Make the music button play/pause
                    case KEY_E:
                        printf("KEY_E\n");
                        scanCode = HUIB_MUSIC;
                        break;
                    case KEY_LEFTBRACE:
                        printf("KEY_LEFTBRACE (next track with media focus: %i)\n",  hasMediaAudioFocus ? 1 : 0);
                        if(hasMediaAudioFocus)
                        {
                            scanCode = HUIB_NEXT;
                        }
                        else
                        {
                            pass_key_to_mzd(event.type, event.code, event.value);
                        }
                        break;
                    case KEY_RIGHTBRACE:
                        printf("KEY_RIGHTBRACE (prev track with media focus: %i)\n",  hasMediaAudioFocus ? 1 : 0);
                        if(hasMediaAudioFocus)
                        {
                            scanCode = HUIB_PREV;
                        }
                        else
                        {
                            pass_key_to_mzd(event.type, event.code, event.value);
                        }
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
                    case KEY_RIGHT:
                        printf("KEY_RIGHT\n");
                        scanCode = HUIB_RIGHT;
                        break;
                    case KEY_UP:
                        printf("KEY_UP\n");
                        scanCode = HUIB_UP;
                        break;
                    case KEY_DOWN:
                        printf("KEY_DOWN\n");
                        scanCode = HUIB_DOWN;
                        break;
                    case KEY_N:
                        printf("KEY_N\n");
                        if (isPressed) {
                            scrollAmount = -1;
                        }
                        break;
                    case KEY_M:
                        printf("KEY_M\n");
                        if (isPressed) {
                            scrollAmount = 1;
                        }
                        break;
                    case KEY_HOME:
                        printf("KEY_HOME\n");
                        scanCode = HUIB_HOME;
                        break;
                    case KEY_R: // NAV
                        printf("KEY_R\n");
                        scanCode = HUIB_NAVIGATION;
                        break;
                    case KEY_Z: // CALL ANS
                        printf("KEY_Z\n");
                        scanCode = HUIB_PHONE;
                        break;
                    case KEY_X: // CALL END
                        printf("KEY_X\n");
#ifdef IOGRAB_DEBUG
                        if(hasMediaAudioFocus && isPressed && ioctl(kbd_fd, EVIOCGRAB, 0) < 0)
                        { // This is just for testing although it may be a useful feature if we polish it a little
                            fprintf(stderr, "EVIOCGRAB failed to ungrab %s\n", EVENT_DEVICE_KBD);
                        }
                        else
#endif
                        {	// we can do this since this button does nothing when not on a call
                            scanCode = HUIB_CALLEND;
                        }
                        break;
                    case KEY_T: // FAV
                        printf("KEY_T\n");
                        scanCode = HUIB_PLAYPAUSE;
                        break;
                    }
                    
                    if (isPressed)
                    {
                        pressScanCode = scanCode;
                        time(&pressedSince);
                    }
                    else
                    {
                        time_t now = time(NULL);
                        if (now - pressedSince >= 2)
                        {
                            if (pressScanCode == HUIB_PLAYPAUSE)
                            {
                                callbacks->releaseAudioFocus();
                            }
                            else if (pressScanCode == HUIB_BACK || pressScanCode == HUIB_CALLEND)
                            {
                                callbacks->releaseVideoFocus();
                            }
                            else if (pressScanCode == HUIB_HOME)
                            {
                                callbacks->takeVideoFocus();
                            }

                        }
                        pressScanCode = 0;
                    }

                    if (scanCode != 0 || scrollAmount != 0)
                    {
                        g_hu->hu_queue_command([timeStamp, scanCode, scrollAmount, isPressed, longPress](IHUConnectionThreadInterface& s)
                        {
                            HU::InputEvent inputEvent;
                            inputEvent.set_timestamp(timeStamp);
                            if (scanCode != 0)
                            {
                                HU::ButtonInfo* buttonInfo = inputEvent.mutable_button()->add_button();
                                buttonInfo->set_is_pressed(isPressed);
                                buttonInfo->set_meta(0);
                                buttonInfo->set_long_press(longPress);
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



VideoOutput::VideoOutput(MazdaEventCallbacks* callbacks)
    : callbacks(callbacks)
{
    /* Open Touchscreen Device */
    touch_fd = open(EVENT_DEVICE_TS, O_RDONLY);

    if (touch_fd < 0) {
        fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_TS);
    }

    if (ioctl(touch_fd, EVIOCGRAB, 1) < 0)
    {
        fprintf(stderr, "EVIOCGRAB failed on %s\n", EVENT_DEVICE_TS);
    }

    kbd_fd = open(EVENT_DEVICE_KBD, O_RDONLY);

    if (kbd_fd < 0)
    {
        fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_KBD);
    }

    if (ioctl(kbd_fd, EVIOCGRAB, 1) < 0)
    {
        fprintf(stderr, "EVIOCGRAB failed on %s\n", EVENT_DEVICE_KBD);
    }

    ui_fd = open(EVENT_DEVICE_UI, O_WRONLY | O_NONBLOCK);

    if (ui_fd < 0)
    {
        fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_UI);
    }

    if (ioctl(ui_fd, UI_SET_EVBIT, EV_KEY) < 0)
    {
        fprintf(stderr, "UI_SET_EVBIT failed on %s\n", EV_KEY);
    }
    if (ioctl(ui_fd, UI_SET_KEYBIT, KEY_LEFTBRACE) < 0)
    {
        fprintf(stderr, "UI_SET_KEYBIT failed on %s\n", KEY_LEFTBRACE);
    }
    if (ioctl(ui_fd, UI_SET_KEYBIT, KEY_RIGHTBRACE) < 0)
    {
        fprintf(stderr, "UI_SET_KEYBIT failed on %s\n", KEY_RIGHTBRACE);
    }
    if (ioctl(ui_fd, UI_SET_KEYBIT, KEY_E) < 0)
    {
        fprintf(stderr, "UI_SET_KEYBIT failed on %s\n", KEY_E);
    }
    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "mzd-uinput");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;

    if(write(ui_fd, &uidev, sizeof(uidev)) < 0)
    {
        fprintf(stderr, "Write uidev failed");
    }

    if (ioctl(ui_fd, UI_DEV_CREATE) < 0)
    {
        fprintf(stderr, "UI_DEV_CREATE failed on %s\n", EVENT_DEVICE_UI);
    }

    int quitpiperw[2];
    if (pipe(quitpiperw) < 0) {
        fprintf(stderr, "Pipe failed");
    }
    input_thread_quit_pipe_read = quitpiperw[0];
    input_thread_quit_pipe_write = quitpiperw[1];

    input_thread = std::thread([this](){ input_thread_func(); } );
    //Drop caches before staing new video
    sync();
    std::ofstream ofs("/proc/sys/vm/drop_caches");
    ofs << "3" << std::endl;
    //if we have ASPECT_RATIO_FIX, cut off the bottom black bar
    const char* vid_pipeline_launch = "appsrc name=mysrc is-live=true block=false max-latency=1000000 do-timestamp=true ! queue ! h264parse ! vpudec low-latency=true framedrop=true framedrop-level-mask=0x200 frame-plus=1 ! mfw_isink name=mysink "
    #if ASPECT_RATIO_FIX
    "axis-left=0 axis-top=-20 disp-width=800 disp-height=520"
    #else
    "axis-left=0 axis-top=0 disp-width=800 disp-height=480"
    #endif
    " max-lateness=1000000000 sync=false async=false";

    GError* error = nullptr;
    vid_pipeline = gst_parse_launch(vid_pipeline_launch, &error);

    if (error != NULL) {
        printf("could not construct pipeline: %s\n", error->message);
        g_clear_error (&error);
    }

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(vid_pipeline));
    gst_bus_add_watch(bus, (GstBusFunc)bus_callback, nullptr);
    gst_object_unref(bus);

    vid_src = GST_APP_SRC(gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysrc"));
    vid_sink = GST_ELEMENT(gst_bin_get_by_name (GST_BIN (vid_pipeline), "mysink"));

    gst_app_src_set_stream_type(vid_src, GST_APP_STREAM_TYPE_STREAM);

    gst_element_set_state((GstElement*)vid_pipeline, GST_STATE_PLAYING);
}

VideoOutput::~VideoOutput()
{
    gst_element_set_state((GstElement*)vid_pipeline, GST_STATE_NULL);

    //data we write doesn't matter, wake up touch polling thread
    write(input_thread_quit_pipe_write, &input_thread_quit_pipe_write, sizeof(input_thread_quit_pipe_write));

    printf("waiting for input_thread\n");
    input_thread.join();

    ioctl(ui_fd, UI_DEV_DESTROY);
    close(ui_fd);
    close(touch_fd);
    close(kbd_fd);
    close(input_thread_quit_pipe_write);
    close(input_thread_quit_pipe_read);


    gst_object_unref(vid_pipeline);
    gst_object_unref(vid_src);
    gst_object_unref(vid_sink);
}

void VideoOutput::MediaPacket(uint64_t timestamp, const byte *buf, int len)
{
    GstBuffer * buffer = gst_buffer_new_and_alloc(len);
    memcpy(GST_BUFFER_DATA(buffer), buf, len);
    int ret = gst_app_src_push_buffer(vid_src, buffer);
    if(ret !=  GST_FLOW_OK){
        printf("push buffer returned %d for %d bytes \n", ret, len);
    }
}
