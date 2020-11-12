#include <glib.h>
#include <cstdio>
#include <cstdlib>
#include <gst/gst.h>
#include <csignal>
#include <thread>
#include <unistd.h>


#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include "hu_uti.h"
#include "hu_aap.h"

#include "gps/mzd_gps.h"
#include "hud/hud.h"
#include "wireless/wireless.h"
#include "usb/usb.h"

#include "main.h"
#include "command_server.h"
#include "callbacks.h"
#include "glib_utils.h"
#include "config.h"

#define HMI_BUS_ADDRESS "unix:path=/tmp/dbus_hmi_socket"
#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"


__asm__(".symver realpath1,realpath1@GLIBC_2.11.1");

gst_app_t gst_app;
IHUAnyThreadInterface* g_hu = nullptr;

std::atomic<bool> exiting;

static void nightmode_thread_func(std::condition_variable &quitcv, std::mutex &quitmutex) {
    //Offset so the GPS and NM thread are not perfectly in sync testing each second
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (!exiting) {
        char gpio_value[3];
        FILE *fd = fopen("/sys/class/gpio/CAN_Day_Mode/value", "r");
        if (fd == nullptr) {
            loge("Failed to open CAN_Day_Mode gpio value for reading");
        } else {
            fread(gpio_value, 1, 2, fd);
            int nightmodenow = !atoi(gpio_value);

            if (nightmodenow) {
                logd("It's night now, %i", nightmodenow);
            } else {
                logd("It's day now, %i", nightmodenow);
            }

            // We send nightmode status periodically, otherwise Google Maps
            // doesn't switch to nightmode if it's started late. Even if the
            // other AA UI is already in nightmode.
            g_hu->hu_queue_command([nightmodenow](IHUConnectionThreadInterface &s) {
                HU::SensorEvent sensorEvent;
                sensorEvent.add_night_mode()->set_is_night(nightmodenow);

                s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
            });
        }
        fclose(fd);
        {
            std::unique_lock<std::mutex> lk(quitmutex);
            if (quitcv.wait_for(lk, std::chrono::milliseconds(1000)) == std::cv_status::no_timeout) {
                break;
            }
        }
    }
    logd("Exiting");
}



void shutdown(__attribute__((unused)) int signum){
    exiting = true;
    g_main_loop_quit(gst_app.loop);
}

class BLMSystemClient : public com::jci::blmsystem::Interface_proxy, public DBus::ObjectProxy {
public:
    BLMSystemClient(DBus::Connection &connection, const char *path, const char *name) : DBus::ObjectProxy(connection, path, name) {}

    void NotifyStateTransition(const uint32_t &current_state, const uint32_t &target_state) override {}

    void NotifyShutdown(const uint32_t &generic_reboot_flags) override {}

    void NotifyAccChange(const int32_t &acc_state, const uint32_t &system_state) override {}

    void NotifySystemStateChange(const uint32_t &old_state, const uint32_t &current_state) override {
        if(current_state >= 4){
            logd("Got Shutdown Signal\n");
            shutdown(SIGINT);
        }
    }
};



int run(int mode, DBus::Connection& serviceBus, DBus::Connection& hmiBus){
    MazdaCommandServerCallbacks commandCallbacks;
    CommandServer commandServer(commandCallbacks);
    printf("headunit version: %s \n", commandCallbacks.GetVersion().c_str());
    if (!commandServer.Start())
    {
        loge("Command server failed to start");
        return 1;
    }


    MazdaEventCallbacks callbacks(serviceBus, hmiBus);
    HUServer headunit(callbacks);
    g_hu = &headunit.GetAnyThreadInterface();
    commandCallbacks.eventCallbacks = &callbacks;

    std::condition_variable quitcv;
    std::mutex quitmutex;
    std::mutex hudmutex;
    int ret;
    if(mode == 1){
        ret = headunit.hu_aap_start(HU_TRANSPORT_TYPE::USB, config::phoneIpAddress, true);
    } else if (mode ==2 ){
        std::string phoneIpAddress("0.0.0.0");
        ret = headunit.hu_aap_start(HU_TRANSPORT_TYPE::WIFI, phoneIpAddress, true);
    }
    //Wait forever for a connection
    if (ret < 0) {
        loge("Something bad happened");
        return 1;
    }

    gst_app.loop = g_main_loop_new(run_on_thread_main_context, FALSE);
    callbacks.connected = true;

    std::thread nm_thread([&quitcv, &quitmutex](){ nightmode_thread_func(quitcv, quitmutex); } );
    std::thread gp_thread(gps_thread_func, &quitcv, &quitmutex, &serviceBus, &exiting);
    std::thread *hud_thread = nullptr;
    if(hud_installed()){
        hud_thread = new std::thread([&quitcv, &quitmutex, &hudmutex](){ hud_thread_func(quitcv, quitmutex, hudmutex, exiting); } );
    }

    /* Start gstreamer pipeline and main loop */

    printf("Starting Android Auto...\n");

    g_main_loop_run (gst_app.loop);

    callbacks.connected = false;
    callbacks.videoFocus = false;
    callbacks.audioFocus = AudioManagerClient::FocusType::NONE;
    callbacks.inCall = false;

    printf("quitting...\n");
    //wake up night mode  and gps polling threads
    quitcv.notify_all();

    printf("waiting for nm_thread\n");
    nm_thread.join();

    printf("waiting for gps_thread\n");
    mzd_gps2_stop();
    gp_thread.join();


    if(hud_thread != nullptr){
        printf("waiting for hud_thread\n");
        hud_thread->join();
    }

    printf("shutting down\n");

    /* Stop AA processing */
    ret = headunit.hu_aap_shutdown();
    if (ret < 0) {
        printf("hu_aap_shutdown() ret: %d\n", ret);
        return ret;
    }
    while(headunit.running()){
        sleep(1);
    }

    headunit.join();

    printf("Exiting Command Server\n");
    commandServer.Stop();

    printf("Disconnecting Callbacks\n");
    commandCallbacks.eventCallbacks = nullptr;

    printf("Cleaning up\n");
    g_main_loop_unref(gst_app.loop);
    gst_app.loop = nullptr;
    g_hu = nullptr;
    sleep(2);
    return 0;
}

int main (int argc, char *argv[])
{
    exiting = false;
    signal(SIGINT, shutdown);
    //Force line-only buffering so we can see the output during hangs
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IOLBF, 0);

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    hu_log_library_versions();
    hu_install_crash_handler();

    DBus::_init_threading();

    gst_init(&argc, &argv);
    int ret;
    try
    {

        if (argc >= 2 && strcmp(argv[1], "test") == 0)
        {
            //test mode from the installer, if we got here it's ok
            printf("###TESTMODE_OK###\n");
            return 0;
        }



        config::readConfig();
        printf("Looping\n");
        while (!exiting)
        {
            std::atomic<bool> detection_done(false);
            std::promise<int> promiseObj;
            std::shared_future<int> futureObj = promiseObj.get_future();
            //This needs to be started before we headunit starts waiting for a connection
            std::thread wireless_handle(wireless_thread, &promiseObj);
            std::thread usb_thread(udev_thread_func, &promiseObj, &detection_done);

            int mode = futureObj.get();
            detection_done.store(true, std::memory_order_relaxed);

            wireless_stop();
            wireless_handle.join();
            usb_thread.join();


            //Make a new one instead of using the default so we can clean it up each run
            run_on_thread_main_context = g_main_context_new();
            //Recreate this each time, it makes the error handling logic simpler
            DBus::Glib::BusDispatcher dispatcher;
            dispatcher.attach(run_on_thread_main_context);
            printf("DBus::Glib::BusDispatcher attached\n");

            DBus::default_dispatcher = &dispatcher;

            printf("Making debug connections\n");
            DBus::Connection hmiBus(HMI_BUS_ADDRESS, false);
            hmiBus.register_bus();

            DBus::Connection serviceBus(SERVICE_BUS_ADDRESS, false);
            serviceBus.register_bus();

            static auto *blmsystem_client = new BLMSystemClient(serviceBus, "/com/jci/blm/system", "com.jci.blmsystem.Interface");

            ret = run(mode,serviceBus, hmiBus);

            g_main_context_unref(run_on_thread_main_context);
            run_on_thread_main_context = nullptr;

            printf("Disconnecting DBus\n");
            hmiBus.disconnect();
            serviceBus.disconnect();
//            DBus::default_dispatcher = nullptr;
        }
    }
    catch(DBus::Error& error)
    {
        loge("DBUS Error: %s: %s", error.name(), error.message());
        return 1;
    }

    return ret;
}
