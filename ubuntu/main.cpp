#include <glib.h>
#include <stdio.h>
#define GDK_VERSION_MIN_REQUIRED (GDK_VERSION_3_10)
#include <gdk/gdk.h>
#include <time.h>

//This gets defined by SDL and breaks the protobuf headers
#undef Status

#include "hu_uti.h"
#include "hu_aap.h"

#include "main.h"
#include "outputs.h"
#include "callbacks.h"

gst_app_t gst_app;

float g_dpi_scalefactor = 1.0f;

IHUAnyThreadInterface* g_hu = nullptr;

uint64_t
get_cur_timestamp() {
        struct timespec tp;
        /* Fetch the time stamp */
        clock_gettime(CLOCK_REALTIME, &tp);

        return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
}

static int
gst_loop(gst_app_t *app) {
        int ret;
        GstStateChangeReturn state_ret;

        app->loop = g_main_loop_new(NULL, FALSE);
        printf("Starting Android Auto...\n");
        g_main_loop_run(app->loop);

        return ret;
}


int
main(int argc, char *argv[]) {

        GOOGLE_PROTOBUF_VERIFY_VERSION;

        hu_log_library_versions();
        hu_install_crash_handler();
#if defined GDK_VERSION_3_10
        printf("GTK VERSION 3.10.0 or higher\n");
        //Assuming we are on Gnome, what's the DPI scale factor?
        gdk_init(&argc, &argv);

        GdkScreen * primaryDisplay = gdk_screen_get_default();
        if (primaryDisplay) {
                g_dpi_scalefactor = (float) gdk_screen_get_monitor_scale_factor(primaryDisplay, 0);
                printf("Got gdk_screen_get_monitor_scale_factor() == %f\n", g_dpi_scalefactor);
        }
#else
        printf("Using hard coded scalefactor\n");
        g_dpi_scalefactor = 1;
#endif
        gst_app_t *app = &gst_app;
        int ret = 0;
        errno = 0;

        gst_init(NULL, NULL);
        struct sigaction action;
        sigaction(SIGINT, NULL, &action);
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
            SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
            return 1;
        }

        sigaction(SIGINT, &action, NULL);

        DesktopCommandServerCallbacks commandCallbacks;
        CommandServer commandServer(commandCallbacks);
        if (!commandServer.Start())
        {
            loge("Command server failed to start");
        }

        //loop to emulate the caar
        while(true)
        {
            DesktopEventCallbacks callbacks;
            HUServer headunit(callbacks);

            /* Start AA processing */
            ret = headunit.hu_aap_start(HU_TRANSPORT_TYPE::USB, true);
            if (ret < 0) {
                    printf("Phone is not connected. Connect a supported phone and restart.\n");
                    return 0;
            }

            callbacks.connected = true;

            g_hu = &headunit.GetAnyThreadInterface();
            commandCallbacks.eventCallbacks = &callbacks;

              /* Start gstreamer pipeline and main loop */
            ret = gst_loop(app);
            if (ret < 0) {
                    printf("STATUS:gst_loop() ret: %d\n", ret);
            }

            callbacks.connected = false;
            commandCallbacks.eventCallbacks = nullptr;

            /* Stop AA processing */
            ret = headunit.hu_aap_shutdown();
            if (ret < 0) {
                    printf("STATUS:hu_aap_stop() ret: %d\n", ret);
                    SDL_Quit();
                    return (ret);
            }

            g_hu = nullptr;
        }

        SDL_Quit();

        return (ret);
}
