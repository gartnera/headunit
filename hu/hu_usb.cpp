#define LOGTAG "hu_usb"
#include "hu_uti.h"  // Utilities
#include "hu_usb.h"
#include <vector>
#include <algorithm>


#include <libusb.h>

#ifndef LIBUSB_LOG_LEVEL_NONE
#define LIBUSB_LOG_LEVEL_NONE     0
#endif
#ifndef LIBUSB_LOG_LEVEL_ERROR
#define LIBUSB_LOG_LEVEL_ERROR    1
#endif
#ifndef LIBUSB_LOG_LEVEL_WARNING
#define LIBUSB_LOG_LEVEL_WARNING  2
#endif
#ifndef LIBUSB_LOG_LEVEL_INFO
#define LIBUSB_LOG_LEVEL_INFO     3
#endif
#ifndef LIBUSB_LOG_LEVEL_DEBUG
#define LIBUSB_LOG_LEVEL_DEBUG    4
#endif

static unsigned char AAP_VAL_MAN[] =  "Android";
static unsigned char AAP_VAL_MOD[] =  "Android Auto";    // "Android Open Automotive Protocol"
static unsigned char AAP_VAL_DESC[] =  "Android Auto";
static unsigned char AAP_VAL_VER[] =  "2.0.1";
static unsigned char AAP_VAL_URI[] =  "https://github.com/gartnera/headunit";
static unsigned char AAP_VAL_SERIAL[] =  "HU-AAAAAA001";

#define ACC_IDX_MAN    0   // Manufacturer
#define ACC_IDX_MOD    1   // Model
#define ACC_IDX_DESC   2  // Model
#define ACC_IDX_VER    3   // Model
#define ACC_IDX_URI    4   // Model
#define ACC_IDX_SERIAL 5   // Model

#define ACC_REQ_GET_PROTOCOL        51
#define ACC_REQ_SEND_STRING         52
#define ACC_REQ_START               53

#define VEN_ID_GOOGLE           0x18D1
#define DEV_ID_OAP              0x2D00
#define DEV_ID_OAP_WITH_ADB     0x2D01

#define USB_DIR_IN              0x80
#define USB_DIR_OUT             0x00
#define USB_TYPE_VENDOR         0x40

struct usbvpid {
  uint16_t vendor;
  uint16_t product;
};

const char * iusb_error_get (int error) {
  #if CMU
   switch (error)
      {
      case LIBUSB_SUCCESS:
              return "Success";
      case LIBUSB_ERROR_IO:
              return "Input/output error";
      case LIBUSB_ERROR_INVALID_PARAM:
              return "Invalid parameter";
      case LIBUSB_ERROR_ACCESS:
              return "Access denied (insufficient permissions)";
      case LIBUSB_ERROR_NO_DEVICE:
              return "No such device (it may have been disconnected)";
      case LIBUSB_ERROR_NOT_FOUND:
              return "Entity not found";
      case LIBUSB_ERROR_BUSY:
              return "Resource busy";
      case LIBUSB_ERROR_TIMEOUT:
              return "Operation timed out";
      case LIBUSB_ERROR_OVERFLOW:
              return "Overflow";
      case LIBUSB_ERROR_PIPE:
              return "Pipe error";
      case LIBUSB_ERROR_INTERRUPTED:
              return "System call interrupted (perhaps due to signal)";
      case LIBUSB_ERROR_NO_MEM:
              return "Insufficient memory";
      case LIBUSB_ERROR_NOT_SUPPORTED:
              return "Operation not supported or unimplemented on this platform";
      case LIBUSB_ERROR_OTHER:
              return "Other error";
      }
      return "Unknown error";
  #else
  return libusb_strerror((libusb_error)error);
  #endif
}

int HUTransportStreamUSB::Write(const byte * buf, int len, int tmo) {

  byte* copy_buf = (byte*)malloc(len);
  memcpy(copy_buf, buf, len);

  libusb_transfer *transfer = libusb_alloc_transfer(0);
  libusb_fill_bulk_transfer(transfer, iusb_dev_hndl, iusb_ep_out,
    copy_buf, len, &libusb_callback_send_tramp, this, 0);

  int iusb_state = libusb_submit_transfer(transfer);
  if (iusb_state < 0)
  {
    loge("  Failed: libusb_submit_transfer: %d (%s)", iusb_state, iusb_error_get (iusb_state));
    libusb_free_transfer(transfer);
    return -1;
  }
  else
  {
    logd(" libusb_submit_transfer for %d bytes", len);
  }
  return len;
}

static int iusb_control_transfer (libusb_device_handle * usb_hndl, uint8_t req_type, uint8_t req_val, uint16_t val, uint16_t idx, byte * buf, uint16_t len, unsigned int tmo) {

  if (ena_log_verbo)
    logd ("Start usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_hndl, req_type, req_val, val, idx, buf, len, tmo);

  int usb_err = libusb_control_transfer (usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
  if (usb_err < 0) {
    //this is too spammy while detecting devices
    //loge ("Error usb_err: %d (%s)  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, iusb_error_get (usb_err), usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
    return (-1);
  }
  if (ena_log_verbo)
    logd ("Done usb_err: %d  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
  return (0);
}

//based on http://source.android.com/devices/accessories/aoa.html
libusb_device_handle* HUTransportStreamUSB::find_oap_device()
{
    libusb_device_handle* handle = libusb_open_device_with_vid_pid(iusb_ctx, VEN_ID_GOOGLE, DEV_ID_OAP);
    if (!handle)
    {
        //try with ADB
        handle = libusb_open_device_with_vid_pid(iusb_ctx, VEN_ID_GOOGLE, DEV_ID_OAP_WITH_ADB);
    }
    return handle;
}

HUTransportStreamUSB::HUTransportStreamUSB()
{

}

HUTransportStreamUSB::~HUTransportStreamUSB()
{
  if (iusb_state != hu_STATE_STOPPED)
  {
    Stop();
  }
}

int HUTransportStreamUSB::Stop() {
  iusb_state = hu_STATE_STOPPIN;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));


  close(readfd);
  close(pipe_write_fd);
  readfd = -1;
  pipe_write_fd = -1;

  close(errorfd);
  close(error_write_fd);
  errorfd = -1;
  error_write_fd = -1;

  if (abort_usb_thread_pipe_write_fd >= 0)
  {
    write(abort_usb_thread_pipe_write_fd, &abort_usb_thread_pipe_write_fd, 1);
  }

  if (usb_recv_thread.joinable())
  {
    usb_recv_thread.join();
  }
  close(abort_usb_thread_pipe_write_fd);
  close(abort_usb_thread_pipe_read_fd);
  abort_usb_thread_pipe_write_fd = -1;
  abort_usb_thread_pipe_read_fd = -1;

  iusb_ep_in = -1;
  iusb_ep_out = -1;

  if (iusb_dev_hndl != NULL)
  {
    int usb_err = libusb_release_interface (iusb_dev_hndl, 0);          // Can get a crash inside libusb_release_interface()
    if (usb_err != 0)
      loge ("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    else
      logd ("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

    libusb_reset_device (iusb_dev_hndl);

    libusb_close (iusb_dev_hndl);
    logd ("Done libusb_close");
    iusb_dev_hndl = NULL;
  }

  if (iusb_ctx)
  {
    libusb_exit (iusb_ctx); // Put here or can get a crash from pulling cable
    iusb_ctx = nullptr;
  }

  iusb_state = hu_STATE_STOPPED;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
  return 0;
}


void HUTransportStreamUSB::usb_recv_thread_main()
{
  pthread_setname_np(pthread_self(), "usb_recv_thread_main");

  timeval zero_tv;
  memset(&zero_tv, 0, sizeof(zero_tv));

  while(poll(usb_thread_event_fds.data(), usb_thread_event_fds.size(), -1) >= 0)
  {
    //wakeup, something happened
    if (usb_thread_event_fds[0].revents == usb_thread_event_fds[0].events)
    {
        logw("Requested to exit");
        break;
    }
    int iusb_state = libusb_handle_events_timeout_completed(iusb_ctx, &zero_tv, nullptr);
    if (iusb_state)
    {
      break;
    }
  }
  logw("libusb_handle_events_completed: %d (%s)", iusb_state, state_get (iusb_state));

  logw("USB thread exit");

  //Wake up the reader if required
  int errData = -1;
  write(error_write_fd, &errData, sizeof(errData));

}

void HUTransportStreamUSB::libusb_callback(libusb_transfer *transfer)
{
  logd("libusb_callback %d %d %d", transfer->status, LIBUSB_TRANSFER_COMPLETED, LIBUSB_TRANSFER_OVERFLOW);
  libusb_transfer_status recv_last_status = transfer->status;
  if (recv_last_status == LIBUSB_TRANSFER_COMPLETED || recv_last_status == LIBUSB_TRANSFER_OVERFLOW)
  {
    if (recv_last_status == LIBUSB_TRANSFER_OVERFLOW)
    {
      logw("LIBUSB_TRANSFER_OVERFLOW");
      recv_temp_buffer.resize(recv_temp_buffer.size() * 2);
      start_usb_recv();
    }
    else
    {
      size_t bytesToWrite = transfer->actual_length;
      unsigned char *buffer = transfer->buffer;

      ssize_t ret = 0;
      while (bytesToWrite > 0)
      {
        ret = write(pipe_write_fd, buffer, bytesToWrite);
        if (ret < 0)
          break;
        logd("Wrote %d of %d bytes", ret, transfer->actual_length);
        buffer += ret;
        bytesToWrite -= ret;
      }

      if (ret < 0)
      {
        loge("libusb_callback: write failed");
        write(abort_usb_thread_pipe_write_fd, &abort_usb_thread_pipe_write_fd, 1);
      }
      else
      {
        start_usb_recv();
      }
    }
  }
  else
  {
    loge("libusb_callback: abort");
    write(abort_usb_thread_pipe_write_fd, &abort_usb_thread_pipe_write_fd, 1);
  }
  libusb_free_transfer(transfer);
}

void HUTransportStreamUSB::libusb_callback_tramp(libusb_transfer *transfer)
{
  reinterpret_cast<HUTransportStreamUSB*>(transfer->user_data)->libusb_callback(transfer);
}


void HUTransportStreamUSB::libusb_callback_send(libusb_transfer *transfer)
{
  logd("libusb_callback_send %d %d %d", transfer->status, LIBUSB_TRANSFER_COMPLETED, LIBUSB_TRANSFER_OVERFLOW);
  libusb_transfer_status recv_last_status = transfer->status;
  if (recv_last_status != LIBUSB_TRANSFER_COMPLETED)
  {
    loge("libusb_callback: abort");
    write(abort_usb_thread_pipe_write_fd, &abort_usb_thread_pipe_write_fd, 1);
  }
  free(transfer->buffer);
  libusb_free_transfer(transfer);
}

void HUTransportStreamUSB::libusb_callback_send_tramp(libusb_transfer *transfer)
{
  reinterpret_cast<HUTransportStreamUSB*>(transfer->user_data)->libusb_callback_send(transfer);
}


int HUTransportStreamUSB::start_usb_recv()
{
    libusb_transfer *transfer = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(transfer, iusb_dev_hndl, iusb_ep_in,
      recv_temp_buffer.data(), recv_temp_buffer.size(), &libusb_callback_tramp, this, 0);

    int iusb_state = libusb_submit_transfer(transfer);
    if (iusb_state < 0)
    {
      loge("  Failed: libusb_submit_transfer: %d (%s)", iusb_state, iusb_error_get (iusb_state));
      libusb_free_transfer(transfer);
    }
    else
    {
      logd(" libusb_submit_transfer for %d bytes", recv_temp_buffer.size());
    }
    return iusb_state;
}

int HUTransportStreamUSB::Start(bool waitForDevice) {

  if (iusb_state == hu_STATE_STARTED) {
    logd ("CHECK: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
    return (0);
  }

  iusb_state = hu_STATE_STARTIN;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));

  if (libusb_init(&iusb_ctx) < 0)
  {
      loge ("Error libusb_init usb_err failed");
      Stop();
      return (-1);
  }

  libusb_set_debug(iusb_ctx, LIBUSB_LOG_LEVEL_INFO);

  //See if there is a OAP device already
  while ((iusb_dev_hndl = find_oap_device()) == nullptr)
  {
    libusb_device** devices = nullptr;
    ssize_t dev_count = libusb_get_device_list(iusb_ctx, &devices);
    if (dev_count < 0)
    {
      loge ("Error libusb_get_device_list usb_err: %d (%s)", dev_count, iusb_error_get (dev_count));
      Stop();
      return (-1);
    }

    bool tried_any = false;
    for (ssize_t i = 0; i < dev_count && !tried_any; i++)
    {
        libusb_device_descriptor desc;
        int usb_err = libusb_get_device_descriptor(devices[i], &desc);
        if (usb_err < 0)
        {
            loge("Error getting descriptor");
            continue;
        }
        logw("Opening device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
        libusb_device_handle* handle = nullptr;
        usb_err = libusb_open(devices[i], &handle);
        if (usb_err < 0)
        {
            loge("Error opening device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
            continue;
        }

        uint16_t oap_proto_ver = 0;
        if (iusb_control_transfer(handle, USB_DIR_IN | USB_TYPE_VENDOR, ACC_REQ_GET_PROTOCOL, 0, 0, (byte*)&oap_proto_ver, sizeof(uint16_t), 1000) >= 0)
        {
            oap_proto_ver = le16toh(oap_proto_ver);
            if (oap_proto_ver < 1)
            {
                continue;
            }
            logw("Device 0x%04x : 0x%04x responded with protocol ver %u", desc.idVendor, desc.idProduct, oap_proto_ver);
            usb_err = iusb_control_transfer(handle, USB_DIR_OUT | USB_TYPE_VENDOR, ACC_REQ_SEND_STRING, 0, ACC_IDX_MAN, AAP_VAL_MAN, sizeof(AAP_VAL_MAN), 1000);
            if (usb_err < 0)
            {
                loge("Error sending ACC_IDX_MAN to device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
                continue;
            }
            usb_err = iusb_control_transfer(handle, USB_DIR_OUT | USB_TYPE_VENDOR, ACC_REQ_SEND_STRING, 0, ACC_IDX_MOD, AAP_VAL_MOD, sizeof(AAP_VAL_MOD), 1000);
            if (usb_err < 0)
            {
                loge("Error sending ACC_IDX_MOD to device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
                continue;
            }
            usb_err = iusb_control_transfer(handle, USB_DIR_OUT | USB_TYPE_VENDOR, ACC_REQ_SEND_STRING, 0, ACC_IDX_DESC, AAP_VAL_DESC, sizeof(AAP_VAL_DESC), 1000);
            if (usb_err < 0)
            {
                loge("Error sending ACC_IDX_DESC to device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
                continue;
            }
            usb_err = iusb_control_transfer(handle, USB_DIR_OUT | USB_TYPE_VENDOR, ACC_REQ_SEND_STRING, 0, ACC_IDX_VER, AAP_VAL_VER, sizeof(AAP_VAL_VER), 1000);
            if (usb_err < 0)
            {
                loge("Error sending ACC_IDX_VER to device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
                continue;
            }
            usb_err = iusb_control_transfer(handle, USB_DIR_OUT | USB_TYPE_VENDOR, ACC_REQ_SEND_STRING, 0, ACC_IDX_URI, AAP_VAL_URI, sizeof(AAP_VAL_URI), 1000);
            if (usb_err < 0)
            {
                loge("Error sending ACC_IDX_URI to device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
                continue;
            }
            usb_err = iusb_control_transfer(handle, USB_DIR_OUT | USB_TYPE_VENDOR, ACC_REQ_SEND_STRING, 0, ACC_IDX_SERIAL, AAP_VAL_SERIAL, sizeof(AAP_VAL_SERIAL), 1000);
            if (usb_err < 0)
            {
                loge("Error sending ACC_IDX_SERIAL to device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
                continue;
            }
            usb_err = iusb_control_transfer(handle, USB_DIR_OUT | USB_TYPE_VENDOR, ACC_REQ_START, 0, 0, nullptr, 0, 1000);
            if (usb_err < 0)
            {
                loge("Error sending ACC_REQ_START to device 0x%04x : 0x%04x", desc.idVendor, desc.idProduct);
                continue;
            }

            tried_any = true;
        }

        libusb_close(handle);
    }

    //unref the devices
    libusb_free_device_list(devices, 1);

    if (tried_any)
    {
        //Try right away just incase
        if ((iusb_dev_hndl = find_oap_device()) == nullptr)
        {
            logw("Wating for the device to reconnect");
            //Give it some time to reconnect
            wait_for_device_connection();
        }
    }
    else
    {
        if (waitForDevice)
        {
            logw("Nothing found, waiting");
            wait_for_device_connection();
        }
        else
        {
            loge ("Can't find any OAP devices");
            Stop();
            return (-1);
        }
    }
  }

  logw("Found OAP Device");

  int usb_err = libusb_claim_interface (iusb_dev_hndl, 0);
  if (usb_err) {
    loge ("Error libusb_claim_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    Stop();
    return (-1);
  }
  logw ("OK libusb_claim_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

  libusb_device* got_device = libusb_get_device(iusb_dev_hndl);

  //OAP uses config 0 for normal operation
  struct libusb_config_descriptor * config = NULL;
  usb_err = libusb_get_config_descriptor (got_device, 0, &config);
  if (usb_err != 0) {
    loge ("Error libusb_get_active_config_descriptor usb_err: %d (%s)  errno: %d (%s)", usb_err, iusb_error_get (usb_err), errno, strerror (errno));
    Stop();
    return (-1);
  }

  int num_int = config->bNumInterfaces;                               // Get number of interfaces
  logw ("Done get_config_descriptor config: %p  num_int: %d", config, num_int);

  for (int idx = 0; idx < num_int && (iusb_ep_in < 0 || iusb_ep_out < 0); idx ++)
  {                              // For all interfaces...
    const libusb_interface& inter = config->interface[idx];
    int num_altsetting = inter.num_altsetting;
    logd ("num_altsetting: %d", num_altsetting);
    for (int j = 0; j < inter.num_altsetting && (iusb_ep_in < 0 || iusb_ep_out < 0); j ++)
    {                    // For all alternate settings...
      const libusb_interface_descriptor& interdesc = inter.altsetting[j];
      int num_int = interdesc.bInterfaceNumber;
      logd ("num_int: %d", num_int);
      int num_eps = interdesc.bNumEndpoints;
      logd ("num_eps: %d", num_eps);
      for (int k = 0; k < num_eps && (iusb_ep_in < 0 || iusb_ep_out < 0); k ++)
      {                                // For all endpoints...
        const libusb_endpoint_descriptor& epdesc = interdesc.endpoint[k];
        if (epdesc.bDescriptorType == LIBUSB_DT_ENDPOINT &&
            (epdesc.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK)
        {          // 5
            int ep_add = epdesc.bEndpointAddress;
            if (ep_add & LIBUSB_ENDPOINT_DIR_MASK)
            {
              if (iusb_ep_in < 0) {
                iusb_ep_in = ep_add;                                   // Set input endpoint
                logw ("iusb_ep_in: 0x%02x", iusb_ep_in);
              }
            }
            else
            {
              if (iusb_ep_out < 0) {
                iusb_ep_out = ep_add;                                  // Set output endpoint
                logw ("iusb_ep_out: 0x%02x", iusb_ep_out);
              }
            }
        }
      }
    }
  }
  libusb_free_config_descriptor (config);
  if (iusb_ep_in < 0 || iusb_ep_out < 0)
  {
      loge ("Error can't find endpoints");
      Stop();
      return (-1);
  }

  int pipefd[2] = {-1,-1};
  if (pipe(pipefd) < 0)
  {
    loge("Pipe create failed");
    return -1;
  }
  readfd = pipefd[0];
  pipe_write_fd = pipefd[1];

  if (pipe(pipefd) < 0)
  {
    loge("Error pipe create failed");
    return -1;
  }
  errorfd = pipefd[0];
  error_write_fd = pipefd[1];

  if (pipe(pipefd) < 0)
  {
    loge("Error pipe create failed");
    return -1;
  }
  abort_usb_thread_pipe_read_fd = pipefd[0];
  abort_usb_thread_pipe_write_fd = pipefd[1];
  //Add entry for our cancel fd
  pollfd abort_poll;
  abort_poll.fd = abort_usb_thread_pipe_read_fd;
  abort_poll.events = POLLIN;
  abort_poll.revents = 0;
  usb_thread_event_fds.push_back(abort_poll);


  const libusb_pollfd** existing_poll_fds = libusb_get_pollfds(iusb_ctx);
  for (auto cur_poll_fd_ptr = existing_poll_fds; *cur_poll_fd_ptr; cur_poll_fd_ptr++)
  {
      auto cur_poll_fd = *cur_poll_fd_ptr;
      pollfd new_poll;
      new_poll.fd = cur_poll_fd->fd;
      new_poll.events = cur_poll_fd->events;
      new_poll.revents = 0;

      usb_thread_event_fds.push_back(new_poll);
  }
#if LIBUSB_API_VERSION >= 0x01000104
  libusb_free_pollfds(existing_poll_fds);
#endif

  libusb_set_pollfd_notifiers(iusb_ctx, &libusb_callback_pollfd_added_tramp, &libusb_callback_pollfd_removed_tramp, this);

  usb_recv_thread = std::thread([this]{ this->usb_recv_thread_main(); });

  recv_temp_buffer.resize(16384);
  start_usb_recv();

  iusb_state = hu_STATE_STARTED;
  logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
  return (0);
}

void HUTransportStreamUSB::libusb_callback_pollfd_added(int fd, short events)
{
    pollfd new_poll;
    new_poll.fd = fd;
    new_poll.events = events;
    new_poll.revents = 0;

    usb_thread_event_fds.push_back(new_poll);
}

void HUTransportStreamUSB::libusb_callback_pollfd_added_tramp(int fd, short events, void* user_data)
{
    reinterpret_cast<HUTransportStreamUSB*>(user_data)->libusb_callback_pollfd_added(fd, events);
}

void HUTransportStreamUSB::libusb_callback_pollfd_removed(int fd)
{
    usb_thread_event_fds.erase(std::remove_if(usb_thread_event_fds.begin(),
                                              usb_thread_event_fds.end(),
                                              [fd](pollfd& p) { return p.fd == fd; }),
                                usb_thread_event_fds.end());
}

void HUTransportStreamUSB::libusb_callback_pollfd_removed_tramp(int fd, void* user_data)
{
    reinterpret_cast<HUTransportStreamUSB*>(user_data)->libusb_callback_pollfd_removed(fd);
}
