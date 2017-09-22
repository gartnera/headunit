#include "outputs.h"
#include "main.h"

static /* Print all information about a key event */
void
PrintKeyInfo(SDL_KeyboardEvent *key) {
        /* Is it a release or a press? */
        if (key->type == SDL_KEYUP)
                printf("Release:- ");
        else
                printf("Press:- ");

        /* Print the hardware scancode first */
        printf("Scancode: 0x%02X", key->keysym.scancode);
        /* Print the name of the key */
        printf(", Name: %s", SDL_GetKeyName(key->keysym.sym));
        /* We want to print the unicode info, but we need to make */
        /* sure its a press event first (remember, release events */
        /* don't have unicode info                                */
        if (key->type == SDL_KEYDOWN) {
                /* If the Unicode value is less than 0x80 then the    */
                /* unicode value can be used to get a printable       */
                /* representation of the key, using (char)unicode.    */
                printf(", Unicode: ");
                if (key->keysym.scancode < 0x80 && key->keysym.scancode > 0) {
                        printf("%c (0x%04X)", (char) key->keysym.scancode,
                                key->keysym.scancode);
                } else {
                        printf("? (0x%04X)", key->keysym.scancode);
                }
        }
        printf("\n");
}

gboolean VideoOutput::bus_callback(GstBus *bus, GstMessage *message, gpointer *ptr) {
    gst_app_t *app = (gst_app_t*) ptr;

    switch (GST_MESSAGE_TYPE(message)) {

    case GST_MESSAGE_ERROR:
    {
        gchar *debug;
        GError *err;

        gst_message_parse_error(message, &err, &debug);
        g_print("Error %s\n", err->message);
        g_error_free(err);
        g_free(debug);
        g_main_loop_quit(app->loop);
    }
        break;

    case GST_MESSAGE_WARNING:
    {
        gchar *debug;
        GError *err;
        gchar *name;

        gst_message_parse_warning(message, &err, &debug);
        g_print("Warning %s\nDebug %s\n", err->message, debug);

        name = (gchar *) GST_MESSAGE_SRC_NAME(message);

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
        //                     g_print("got message %s\n", \
        gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
        break;
    }

    return TRUE;
}

void VideoOutput::aa_touch_event(SDL_Window *window, HU::TouchInfo::TOUCH_ACTION action, unsigned int x, unsigned int y) {
    int windowW, windowH;

    SDL_GetWindowSize(window, &windowW, &windowH);
    float normx = float(x) / float(windowW);
    float normy = float(y) / float(windowH);

    x = (unsigned int) (normx * 800);
#if ASPECT_RATIO_FIX
    y = (unsigned int) (normy * 450) + 15;
#else
    y = (unsigned int) (normy * 480);
#endif

    g_hu->hu_queue_command([action, x, y](IHUConnectionThreadInterface & s) {
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

gboolean VideoOutput::sdl_poll_event_wrapper(gpointer data){
    return reinterpret_cast<VideoOutput*>(data)->sdl_poll_event();
}

gboolean VideoOutput::sdl_poll_event() {

    SDL_Event event;
    SDL_MouseButtonEvent *mbevent;
    SDL_MouseMotionEvent *mmevent;
    SDL_KeyboardEvent *key;

    bool nightmodenow = nightmode;

    int ret;
    while (SDL_PollEvent(&event) > 0) {
        switch (event.type) {
        case SDL_MOUSEMOTION:
            mmevent = &event.motion;
            if (mmevent->state & SDL_BUTTON_LMASK) {
                aa_touch_event(SDL_GetWindowFromID(mmevent->windowID), HU::TouchInfo::TOUCH_ACTION_DRAG, (unsigned int) mmevent->x, (unsigned int) mmevent->y);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            mbevent = &event.button;
            if (mbevent->button == SDL_BUTTON_LEFT) {
                aa_touch_event(SDL_GetWindowFromID(mbevent->windowID), HU::TouchInfo::TOUCH_ACTION_PRESS, (unsigned int) mbevent->x, (unsigned int) mbevent->y);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            mbevent = &event.button;
            if (mbevent->button == SDL_BUTTON_LEFT) {
                aa_touch_event(SDL_GetWindowFromID(mbevent->windowID), HU::TouchInfo::TOUCH_ACTION_RELEASE, (unsigned int) mbevent->x, (unsigned int) mbevent->y);
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.sym != 0) {
                key = &event.key;
                PrintKeyInfo(key);

                HU::InputEvent inputEvent;
                inputEvent.set_timestamp(get_cur_timestamp());
                HU::ButtonInfo* buttonInfo = inputEvent.mutable_button()->add_button();
                buttonInfo->set_is_pressed(event.type == SDL_KEYDOWN);
                buttonInfo->set_meta(0);
                buttonInfo->set_long_press(false);
                if (key->keysym.sym == SDLK_UP) {
                    buttonInfo->set_scan_code(HUIB_UP);
                } else if (key->keysym.sym == SDLK_DOWN) {
                    buttonInfo->set_scan_code(HUIB_DOWN);
                } else if (key->keysym.sym == SDLK_TAB) { //Left is the menu, so kinda tab?
                    buttonInfo->set_scan_code(HUIB_LEFT);
                }//This is just mic again
                // else if (key->keysym.sym == SDLK_RIGHT) {
                //      buttonInfo->set_scan_code(HUIB_RIGHT);
                // }
                else if (key->keysym.sym == SDLK_LEFT || key->keysym.sym == SDLK_RIGHT) {
                    if (event.type == SDL_KEYDOWN) {
                        HU::InputEvent inputEvent2;
                        inputEvent2.set_timestamp(get_cur_timestamp());
                        HU::RelativeInputEvent* rel = inputEvent2.mutable_rel_event()->mutable_event();
                        rel->set_delta(key->keysym.sym == SDLK_LEFT ? -1 : 1);
                        rel->set_scan_code(HUIB_SCROLLWHEEL);

                        g_hu->hu_queue_command([inputEvent2](IHUConnectionThreadInterface & s) {
                            s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent2);
                        });
                    }
                } else if (key->keysym.sym == SDLK_m) {
                    buttonInfo->set_scan_code(HUIB_MIC);
                } else if (key->keysym.sym == SDLK_p) {
                    buttonInfo->set_scan_code(HUIB_PREV);
                } else if (key->keysym.sym == SDLK_n) {
                    buttonInfo->set_scan_code(HUIB_NEXT);
                } else if (key->keysym.sym == SDLK_SPACE) {
                    buttonInfo->set_scan_code(HUIB_PLAYPAUSE);
                } else if (key->keysym.sym == SDLK_RETURN) {
                    buttonInfo->set_scan_code(HUIB_ENTER);
                } else if (key->keysym.sym == SDLK_BACKSPACE) {
                    buttonInfo->set_scan_code(HUIB_BACK);
                } else if (key->keysym.sym == SDLK_F1) {
                    if (event.type == SDL_KEYUP) {
                        nightmodenow = !nightmodenow;
                    }
                } else if (key->keysym.sym = SDLK_F2) {
                    if (event.type == SDL_KEYUP) {
                        // Send a fake location in germany
                        HU::SensorEvent sensorEvent;
                        HU::SensorEvent_LocationData* location = sensorEvent.add_location_data();
                        location->set_timestamp(get_cur_timestamp());
                        location->set_latitude(48.562964 * 1E7);
                        location->set_longitude(13.385639 * 1E7);
                        location->set_speed(48 * 1E3);
                        HU::SensorEvent_DrivingStatus* driving = sensorEvent.add_driving_status();
                        driving->set_status(HU::SensorEvent::DrivingStatus::DRIVE_STATUS_NO_KEYBOARD_INPUT
                                            | HU::SensorEvent::DrivingStatus::DRIVE_STATUS_NO_CONFIG
                                            | HU::SensorEvent::DrivingStatus::DRIVE_STATUS_LIMIT_MESSAGE_LEN);

                        g_hu->hu_queue_command([sensorEvent](IHUConnectionThreadInterface& s)
                        {
                            s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
                        });

                        printf("Sending fake location.");
                    }
                }

                if (buttonInfo->has_scan_code()) {
                    g_hu->hu_queue_command([inputEvent](IHUConnectionThreadInterface & s) {
                        s.hu_aap_enc_send_message(0, AA_CH_TOU, HU_INPUT_CHANNEL_MESSAGE::InputEvent, inputEvent);
                    });
                }
            }
            break;

        case SDL_QUIT:
            {
                //we "lost video focus"
                callbacks->VideoFocusHappened(false, VIDEO_FOCUS_REQUESTOR::HEADUNIT);
            }
            break;
        }
    }



    if (nightmode != nightmodenow) {
       nightmode = nightmodenow;
       SendNightMode();
    }

    return TRUE;
}

VideoOutput::VideoOutput(DesktopEventCallbacks* callbacks) : callbacks(callbacks) {
    GstBus *bus;

    GError *error = NULL;

    const char* vid_launch_str = "appsrc name=mysrc is-live=true block=false max-latency=100000 do-timestamp=true stream-type=stream typefind=true ! "
                                 "queue ! "
                                 "h264parse ! "
                                 "avdec_h264 ! "
        #if ASPECT_RATIO_FIX
                                 "videocrop top=16 bottom=15 ! "
        #endif
                                 "videoscale name=myconvert ! "
                                 "videoconvert ! "
                                 "xvimagesink name=mysink";
    vid_pipeline = gst_parse_launch(vid_launch_str, &error);

    bus = gst_pipeline_get_bus(GST_PIPELINE(vid_pipeline));
    gst_bus_add_watch(bus, (GstBusFunc) bus_callback, &gst_app);
    gst_object_unref(bus);

    vid_src = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(vid_pipeline), "mysrc"));

    gst_app_src_set_stream_type(vid_src, GST_APP_STREAM_TYPE_STREAM);


    window = SDL_CreateWindow("Android Auto",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                          #if ASPECT_RATIO_FIX
                              //emulate the CMU stretching
                              (int) (853 * g_dpi_scalefactor), (int) (480 * g_dpi_scalefactor),
                          #else
                              (int) (800 * g_dpi_scalefactor), (int) (480 * g_dpi_scalefactor),
                          #endif
                              SDL_WINDOW_SHOWN);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

    GstVideoOverlay* sink = GST_VIDEO_OVERLAY(gst_bin_get_by_name(GST_BIN(vid_pipeline), "mysink"));
    gst_video_overlay_set_window_handle(sink, wmInfo.info.x11.window);
    //Let SDL do the key events, etc
    gst_video_overlay_handle_events(sink, FALSE);
    gst_object_unref(sink);

    //Don't use SDL's weird cursor, too small on HiDPI
    XUndefineCursor(wmInfo.info.x11.display, wmInfo.info.x11.window);

    timeout_src = g_timeout_source_new(100);
    g_source_set_priority(timeout_src, G_PRIORITY_HIGH);
    g_source_set_callback(timeout_src, &sdl_poll_event_wrapper, (gpointer)this, NULL);
    g_source_attach(timeout_src, NULL);

    gst_element_set_state(vid_pipeline, GST_STATE_PLAYING);

    SendNightMode();
}

VideoOutput::~VideoOutput()
{
    gst_element_set_state(vid_pipeline, GST_STATE_NULL);

    gst_object_unref(vid_pipeline);
    gst_object_unref(vid_src);

    vid_pipeline = nullptr;
    vid_src = nullptr;
    g_source_destroy(timeout_src);
    g_source_unref(timeout_src);
    timeout_src = nullptr;

    SDL_DestroyWindow(window);
    window = nullptr;
}

void VideoOutput::MediaPacket(uint64_t timestamp, const byte *buf, int len) {
    GstBuffer * buffer = gst_buffer_new_and_alloc(len);
    gst_buffer_fill(buffer, 0, buf, len);
    int ret = gst_app_src_push_buffer((GstAppSrc *) vid_src, buffer);
    if (ret != GST_FLOW_OK) {
        printf("push buffer returned %d for %d bytes \n", ret, len);
    }
}

void VideoOutput::SendNightMode()
{
    bool nm = nightmode;
    g_hu->hu_queue_command([nm](IHUConnectionThreadInterface & s) {
        HU::SensorEvent sensorEvent;
        sensorEvent.add_night_mode()->set_is_night(nm);

        s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
    });

    printf("Nightmode: %s\n", nm ? "On" : "Off");
}
