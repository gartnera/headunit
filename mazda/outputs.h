#pragma once

#include <glib.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <dbus-c++/dbus.h>
#include <time.h>
#include <glib-unix.h>

#include "dbus/generated_cmu.h"

//This gets defined by SDL and breaks the protobuf headers
#undef Status

#include "hu_uti.h"
#include "hu_aap.h"

#ifndef ASPECT_RATIO_FIX
#define ASPECT_RATIO_FIX 1
#endif

struct gst_app_t;
class MazdaEventCallbacks;

class VideoOutput {
    GstElement *vid_pipeline = nullptr;
    GstAppSrc *vid_src = nullptr;
    GstElement *vid_sink = nullptr;
    MazdaEventCallbacks* callbacks;

    std::thread input_thread;
    int input_thread_quit_pipe_read = -1;
    int input_thread_quit_pipe_write = -1;
    int touch_fd = -1, kbd_fd = -1;
    void input_thread_func();

public:
    VideoOutput(MazdaEventCallbacks* callbacks);
    ~VideoOutput();

    void MediaPacket(uint64_t timestamp, const byte * buf, int len);
};
