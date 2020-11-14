#include "hu_aap.h"
#include <thread>
#include <libusb-1.0/libusb.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <poll.h>

const char* iusb_error_get(int error);

class HUTransportStreamUSB : public HUTransportStream
{
    HU_STATE isub_state = hu_STATE_INITIAL;

    libusb_context* iusb_ctx = NULL;
    libusb_device_handle * iusb_dev_hndl      = NULL;
    int   iusb_ep_in          = -1;
    int   iusb_ep_out         = -1;

    int pipe_write_fd = -1;
    int error_write_fd = -1;
    int iusb_state = 0; // 0: Initial    1: Startin    2: Started    3: Stoppin    4: Stopped

    int abort_usb_thread_pipe_read_fd = -1;
    int abort_usb_thread_pipe_write_fd = -1;
    std::mutex usb_thread_event_fds_lock;
    std::vector<pollfd> usb_thread_event_fds;

    //usb recv thread state
    std::vector<byte> recv_temp_buffer;
    std::thread usb_recv_thread;
    void usb_recv_thread_main();
    int start_usb_recv();
    void libusb_callback(libusb_transfer *transfer);
    static void libusb_callback_tramp(libusb_transfer *transfer);

    void libusb_callback_send(libusb_transfer *transfer);
    static void libusb_callback_send_tramp(libusb_transfer *transfer);

    void libusb_callback_pollfd_added(int fd, short events);
    static void libusb_callback_pollfd_added_tramp(int fd, short events, void* user_data);

    void libusb_callback_pollfd_removed(int fd);
    static void libusb_callback_pollfd_removed_tramp(int fd, void* user_data);

    libusb_device_handle* find_oap_device();
public:
    ~HUTransportStreamUSB();
    HUTransportStreamUSB();
    virtual int Start(bool waitForDevice) override;
    virtual int Stop() override;
    virtual int Write(const byte* buf, int len, int tmo) override;
};
