
  //

  #define LOGTAG "hu_usb"
  #include "hu_uti.h"                                                  // Utilities
  #include "hu_oap.h"                                                  // Open Accessory Protocol

  int iusb_state = 0; // 0: Initial    1: Startin    2: Started    3: Stoppin    4: Stopped


  #ifdef __ANDROID_API__
  #include <libusb.h>
  #else
  #include <libusb.h>
  #endif

//  #ifdef __ANDROID_API__
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
//  #endif

/* APIs used:

libusb_init
libusb_set_debug
libusb_exit
libusb_open
libusb_close

libusb_get_device_list
libusb_free_device_list

libusb_get_config_descriptor
libusb_free_config_descriptor

libusb_get_device_descriptor

libusb_claim_interface
libusb_release_interface

libusb_control_transfer
libusb_bulk_transfer


Standard libusb to Android libusbhost API mappings:

#define libusb_device                     struct usb_device   ??
#define libusb_device_handle      struct usb_device
#define libusb_context            struct usb_host_context
#define libusb_device_descriptor  const struct usb_device_descriptor

int     LIBUSB_CALL libusb_init                   (libusb_context **ctx);
void    LIBUSB_CALL libusb_exit                   (libusb_context *ctx);
void    LIBUSB_CALL libusb_set_debug              (libusb_context *ctx, int level);
int     LIBUSB_CALL libusb_open                   (libusb_device *dev, libusb_device_handle **handle);
void    LIBUSB_CALL libusb_close                  (libusb_device_handle *dev_handle);

ssize_t LIBUSB_CALL libusb_get_device_list        (libusb_context *ctx, libusb_device ***list);
void    LIBUSB_CALL libusb_free_device_list       (libusb_device **list, int unref_devices);
int     LIBUSB_CALL libusb_get_config_descriptor  (libusb_device *dev, uint8_t config_index, struct libusb_config_descriptor **config);
void    LIBUSB_CALL libusb_free_config_descriptor (struct libusb_config_descriptor *config);
int     LIBUSB_CALL libusb_get_device_descriptor  (libusb_device *dev, struct libusb_device_descriptor *desc);

int     LIBUSB_CALL libusb_claim_interface        (libusb_device_handle *dev, int interface_number);
int     LIBUSB_CALL libusb_release_interface      (libusb_device_handle *dev, int interface_number);
int     LIBUSB_CALL libusb_control_transfer       (libusb_device_handle *dev_handle, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout);
int     LIBUSB_CALL libusb_bulk_transfer          (libusb_device_handle *dev_handle, unsigned char endpoint, unsigned char *data, int length, int *actual_length, unsigned int timeout);

    #define libusb_init                   usb_host_init                       struct usb_host_context * usb_host_init     (void);
    #define libusb_exit                   usb_host_cleanup                    void                      usb_host_cleanup  (struct usb_host_context *context);
-
    #define libusb_open                   usb_device_open                     struct usb_device *       usb_device_open   (const char *dev_name);
#define libusb_close                  usb_device_close                    void                usb_device_close              (struct usb_device *device);

?
?
?
?
#define libusb_get_device_descriptor  usb_device_get_device_descriptor    const struct usb_device_descriptor* usb_device_get_device_descriptor  (struct usb_device *device);

#define libusb_claim_interface        usb_device_claim_interface
#define libusb_release_interface      usb_device_release_interface
#define libusb_control_transfer       usb_device_control_transfer
    #define libusb_bulk_transfer          usb_device_bulk_transfer        int                 usb_device_bulk_transfer      (struct usb_device *device,        int endpoint,           void* buffer,        int length,                     unsigned int timeout);


*/



    // Data: 

  struct libusb_device_handle * iusb_dev_hndl      = NULL;
  libusb_device *               iusb_best_device  = NULL;
  int   iusb_ep_in          = -1;
  int   iusb_ep_out         = -1;

  int   iusb_best_vendor    = 0;
  int   iusb_best_product   = 0;
  byte  iusb_curr_man [256] = {0};
  byte  iusb_best_man [256] = {0};
  byte  iusb_curr_pro [256] = {0};
  byte  iusb_best_pro [256] = {0};

struct usbvpid {
    uint16_t vendor;
    uint16_t product;
};


#ifndef NDEBUG

  char * iusb_error_get (int error) {
    switch (error) {
      case LIBUSB_SUCCESS:                                              // 0
        return ("LIBUSB_SUCCESS");
      case LIBUSB_ERROR_IO:                                             // -1
        return ("LIBUSB_ERROR_IO");
      case LIBUSB_ERROR_INVALID_PARAM:                                  // -2
        return ("LIBUSB_ERROR_INVALID_PARAM");
      case LIBUSB_ERROR_ACCESS:                                         // -3
        return ("LIBUSB_ERROR_ACCESS");
      case LIBUSB_ERROR_NO_DEVICE:                                      // -4
        return ("LIBUSB_ERROR_NO_DEVICE");
      case LIBUSB_ERROR_NOT_FOUND:                                      // -5
        return ("LIBUSB_ERROR_NOT_FOUND");
      case LIBUSB_ERROR_BUSY:                                           // -6
        return ("LIBUSB_ERROR_BUSY");
      case LIBUSB_ERROR_TIMEOUT:                                        // -7
        return ("LIBUSB_ERROR_TIMEOUT");
      case LIBUSB_ERROR_OVERFLOW:                                       // -8
        return ("LIBUSB_ERROR_OVERFLOW");
      case LIBUSB_ERROR_PIPE:                                           // -9
        return ("LIBUSB_ERROR_PIPE");
      case LIBUSB_ERROR_INTERRUPTED:                                    // -10
        return ("Error:LIBUSB_ERROR_INTERRUPTED");
      case LIBUSB_ERROR_NO_MEM:                                         // -11
        return ("LIBUSB_ERROR_NO_MEM");
      case LIBUSB_ERROR_NOT_SUPPORTED:                                  // -12
        return ("LIBUSB_ERROR_NOT_SUPPORTED");
      case LIBUSB_ERROR_OTHER:                                          // -99
        return ("LIBUSB_ERROR_OTHER");
    }
    return ("LIBUSB_ERROR Unknown error");//: %d", error);
  }


  /*
  void iusb_transfer_status_log (int status) {
    switch (status) {
      case LIBUSB_TRANSFER_COMPLETED:
        logd ("LIBUSB_TRANSFER_COMPLETED");
        break;
      case LIBUSB_TRANSFER_ERROR:
        loge ("LIBUSB_TRANSFER_ERROR");
        break;
      case LIBUSB_TRANSFER_TIMED_OUT:
        loge ("LIBUSB_TRANSFER_TIMED_OUT");
        break;
      case LIBUSB_TRANSFER_CANCELLED:
        loge ("LIBUSB_TRANSFER_CANCELLED");
        break;
      case LIBUSB_TRANSFER_STALL:
        loge ("LIBUSB_TRANSFER_STALL");
        break;
      case LIBUSB_TRANSFER_NO_DEVICE:
        loge ("LIBUSB_TRANSFER_NO_DEVICE");
        break;
      case LIBUSB_TRANSFER_OVERFLOW:
        loge ("LIBUSB_TRANSFER_OVERFLOW");
        break;
      default:
        loge ("LIBUSB_TRANSFER Unknown status: %d", status);
        break;
    }
  }
  */
#endif

  int iusb_bulk_transfer (int ep, byte * buf, int len, int tmo) { // 0 = unlimited timeout

    char * dir = "recv";
    if (ep == iusb_ep_out)
      dir = "send";

    if (iusb_state != hu_STATE_STARTED) {
      logd ("CHECK: iusb_bulk_transfer start iusb_state: %d (%s) dir: %s  ep: 0x%02x  buf: %p  len: %d  tmo: %d", iusb_state, state_get (iusb_state), dir, ep, buf, len, tmo);
      return (-1);
    }
    #define MAX_LEN 65536 //16384 //8192  // 65536
    if (ep == iusb_ep_in && len > MAX_LEN)
      len = MAX_LEN;
    if (ena_log_extra) {
      logw ("Start dir: %s  ep: 0x%02x  buf: %p  len: %d  tmo: %d", dir, ep, buf, len, tmo);
    }
//#ifndef NDEBUG
    if (ena_hd_tra_send && ep == iusb_ep_out)
      hex_dump ("US: ", 16, buf, len);
//#endif

    int usb_err = -2;
    int total_bytes_xfrd = 0;
    int bytes_xfrd = 0;
      // Check the transferred parameter for bulk writes. Not all of the data may have been written.

      // Check transferred when dealing with a timeout error code.
      // libusb may have to split your transfer into a number of chunks to satisfy underlying O/S requirements, meaning that the timeout may expire after the first few chunks have completed.
      // libusb is careful not to lose any data that may have been transferred; do not assume that timeout conditions indicate a complete lack of I/O.

    errno = 0;
    int continue_transfer = 1;
    while (continue_transfer) {
      bytes_xfrd = 0;                                                   // Default tranferred = 0

unsigned long ms_start = ms_get ();
      usb_err = libusb_bulk_transfer (iusb_dev_hndl, ep, buf, len, & bytes_xfrd, tmo);
unsigned long ms_duration = ms_get () - ms_start;
if (ms_duration > 400)
  loge ("ms_duration: %d dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", ms_duration, dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));


      continue_transfer = 0;                                            // Default = transfer done

      if (bytes_xfrd > 0)                                               // If bytes were transferred
        total_bytes_xfrd += bytes_xfrd;                                 // Add to total

      if ((total_bytes_xfrd > 0 || bytes_xfrd > 0) && usb_err == LIBUSB_ERROR_TIMEOUT) {          // If bytes were transferred but we had a timeout...
//        continue_transfer = 1;                                          // Continue the transfer
        logd ("CONTINUE dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get (usb_err));
        buf += bytes_xfrd;                                              // For next transfer, point deeper info buf
        len -= bytes_xfrd;                                              // For next transfer, reduce buf len capacity
//        ms_sleep (50);
      }
      else if (usb_err < 0 && usb_err != LIBUSB_ERROR_TIMEOUT)
        loge ("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));
      else if (ena_log_verbo && usb_err != LIBUSB_ERROR_TIMEOUT)// && (ena_hd_tra_send || ep == iusb_ep_in))
        logd ("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));
      else if (ena_log_extra)
        logw ("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));
    }

    if (total_bytes_xfrd > 16384) {                                     // Previously caused problems that seem fixed by raising USB Rx buffer from 16K to 64K
                                                                        // Using a streaming mode (first reading packet length) may be better but may also create sync or other issues
      //loge ("total_bytes_xfrd: %d     ???????????????? !!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ?????????????????????????", total_bytes_xfrd);
    }

    if (total_bytes_xfrd <= 0 && usb_err < 0) {
      if (errno == EAGAIN || errno == ETIMEDOUT || usb_err == LIBUSB_ERROR_TIMEOUT) {
        return (0);
	}
	  
	  loge ("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get (usb_err), errno, strerror (errno));

      hu_usb_stop ();  // Other errors here are fatal, so stop USB
      return (-1);
    }

    //if (ena_log_verbo)
    //  logd ("Done dir: %s  len: %d  total_bytes_xfrd: %d", dir, len, total_bytes_xfrd);

//#ifndef NDEBUG
    if (ena_hd_tra_recv && ep == iusb_ep_in)
      hex_dump ("UR: ", 16, buf, total_bytes_xfrd);
//#endif


    return (total_bytes_xfrd);
  }

  int hu_usb_recv (byte * buf, int len, int tmo) {
    int ret = iusb_bulk_transfer (iusb_ep_in, buf, len, tmo);       // milli-second timeout
    return (ret);
  }

  int hu_usb_send (byte * buf, int len, int tmo) {
    int ret = iusb_bulk_transfer (iusb_ep_out, buf, len, tmo);      // milli-second timeout
    return (ret);
  }

  int iusb_control_transfer (libusb_device_handle * usb_hndl, uint8_t req_type, uint8_t req_val, uint16_t val, uint16_t idx, byte * buf, uint16_t len, unsigned int tmo) {

    //if (ena_log_verbo)
      logd ("Start usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_hndl, req_type, req_val, val, idx, buf, len, tmo);

    int usb_err = libusb_control_transfer (usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
    if (usb_err < 0) {
      loge ("Error usb_err: %d (%s)  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, iusb_error_get (usb_err), usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
      return (-1);
    }
    //if (ena_log_verbo)
      logd ("Done usb_err: %d  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
    return (0);
  }


  int iusb_oap_start () {
    byte oap_protocol_data [2] = {0, 0};
    int oap_protocol_level   = 0;

    uint8_t       req_type= USB_SETUP_DEVICE_TO_HOST | USB_SETUP_TYPE_VENDOR | USB_SETUP_RECIPIENT_DEVICE;
    uint8_t       req_val = ACC_REQ_GET_PROTOCOL;
    uint16_t      val     = 0;
    uint16_t      idx     = 0;
    byte *        data    = oap_protocol_data;
    uint16_t      len     = sizeof (oap_protocol_data);
    unsigned int  tmo     = 1000;
                                                                        // Get OAP Protocol level
    val = 0;
    idx = 0;
    int usb_err = iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, idx, data, len, tmo);
    if (usb_err != 0) {
      loge ("Done iusb_control_transfer usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
      return (-1);
    }

    logd ("Done iusb_control_transfer usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

    oap_protocol_level = data [1] << 8 | data [0];
    if (oap_protocol_level < 2) {
      loge ("Error oap_protocol_level: %d", oap_protocol_level);
      return (-1);
    }
    logd ("oap_protocol_level: %d", oap_protocol_level);
    
    ms_sleep (50);//1);                                                        // Sometimes hangs on the next transfer

//if (iusb_best_vendor == USB_VID_GOO)
//  {logd ("Done USB_VID_GOO 1"); return (0); }

    req_type = USB_SETUP_HOST_TO_DEVICE | USB_SETUP_TYPE_VENDOR | USB_SETUP_RECIPIENT_DEVICE;
    req_val = ACC_REQ_SEND_STRING;

    iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_MAN, AAP_VAL_MAN, strlen (AAP_VAL_MAN) + 1, tmo);
    iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_MOD, AAP_VAL_MOD, strlen (AAP_VAL_MOD) + 1, tmo);
    //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_DES, AAP_VAL_DES, strlen (AAP_VAL_DES) + 1, tmo);
    //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_VER, AAP_VAL_VER, strlen (AAP_VAL_VER) + 1, tmo);
    //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_URI, AAP_VAL_URI, strlen (AAP_VAL_URI) + 1, tmo);
    //iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, ACC_IDX_SER, AAP_VAL_SER, strlen (AAP_VAL_SER) + 1, tmo);

    logd ("Accessory IDs sent");

//if (iusb_best_vendor == USB_VID_GOO)
//  {logd ("Done USB_VID_GOO 2"); return (0); }

/*
    req_val = ACC_REQ_AUDIO;
    val = 1;                                                            // 0 for no audio (default),    1 for 2 channel, 16-bit PCM at 44100 KHz
    idx = 0;
    if (iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, idx, NULL, 0, tmo) < 0) {
      loge ("Error Audio mode request sent");
      return (-1);
    }
    logd ("OK Audio mode request sent");
*/
    req_val = ACC_REQ_START;
    val = 0;
    idx = 0;
    if (iusb_control_transfer (iusb_dev_hndl, req_type, req_val, val, idx, NULL, 0, tmo) < 0) {
      loge ("Error Accessory mode start request sent");
      return (-1);
    }
    logd ("OK Accessory mode start request sent");

    return (0);
  }

  int iusb_deinit () {                                              // !!!! Need to better reset and wait a while to kill transfers in progress and auto-restart properly

    //if (ctrl_transfer != NULL)
    //  libusb_free_transfer (ctrl_transfer);                           // Free all transfers individually

    if (iusb_dev_hndl == NULL) {
      loge ("Done iusb_dev_hndl: %p", iusb_dev_hndl);
      return (-1);
    }

    int usb_err = libusb_release_interface (iusb_dev_hndl, 0);          // Can get a crash inside libusb_release_interface()
    if (usb_err != 0)
      loge ("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    else
      logd ("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

	libusb_reset_device (iusb_dev_hndl);
	
    libusb_close (iusb_dev_hndl);
    logd ("Done libusb_close");
    iusb_dev_hndl = NULL;

    libusb_exit (NULL); // Put here or can get a crash from pulling cable

    logd ("Done");

    return (0);
  }


struct usbvpid iusb_vendor_get (libusb_device * device) {

    struct usbvpid dev = {0,0};

    if (device == NULL)
      return dev;

    struct libusb_device_descriptor desc = {0};

    int usb_err = libusb_get_device_descriptor (device, & desc);
    if (usb_err != 0) {
      loge ("Error usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
      return dev;
    }
        
    dev.vendor  = desc.idVendor;
    dev.product = desc.idProduct;
    logd ("Done usb_err: %d  vendor:product = 0x%04x:0x%04x", usb_err, dev.vendor, dev.product);
    return (dev);
  }

  int iusb_vendor_priority_get (int vendor) {
    if (vendor == USB_VID_GOO)
      return (10);
    if (vendor == USB_VID_HTC)
      return (9);
    if (vendor == USB_VID_MOT)
      return (8);
    if (vendor == USB_VID_SAM)
      return (7);
    if (vendor == USB_VID_SON)
      return (6);
    if (vendor == USB_VID_LGE)
      return (5);
    if (vendor == USB_VID_LGD)
      return (5);
    if (vendor == USB_VID_ACE)
      return (5);
    if (vendor == USB_VID_HUA)
      return (5);
    if (vendor == USB_VID_PAN)
      return (5);
    if (vendor == USB_VID_ZTE)
      return (5);
    if (vendor == USB_VID_GAR)      
      return (5);
    if (vendor == USB_VID_O1A)
      return (4);
    if (vendor == USB_VID_QUA)
      return (3);
    if (vendor == USB_VID_ONE)
      return (5);
    if (vendor == USB_VID_XIA)
      return (5);
    if (vendor == USB_VID_ASU)
      return (5);
    if (vendor == USB_VID_MEI)
      return (5);      
    if (vendor == USB_VID_LEN)
      return (5);      
    if (vendor == USB_VID_OPO)
      return (5);      
    if (vendor == USB_VID_LEE)
      return (5);      
    if (vendor == USB_VID_ZUK)
      return (5);      

    return (0);
  }


  int iusb_init (byte ep_in_addr, byte ep_out_addr) {
    logd ("ep_in_addr: %d  ep_out_addr: %d", ep_in_addr, ep_out_addr);

    iusb_ep_in  = -1;
    iusb_ep_out = -1;
    iusb_best_device = NULL;
    iusb_best_vendor = 0;

    int usb_err = libusb_init (NULL);
    if (usb_err < 0) {
      loge ("Error libusb_init usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
      return (-1);
    }
    logd ("OK libusb_init usb_err: %d", usb_err);

    libusb_set_debug (NULL, LIBUSB_LOG_LEVEL_INFO);    // DEBUG);//
    logd ("Done libusb_set_debug");

    libusb_device ** list;
    usb_err = libusb_get_device_list (NULL, & list);                // Get list of USB devices
    if (usb_err < 0) {
      loge ("Error libusb_get_device_list cnt: %d", usb_err, iusb_error_get (usb_err));
      return (-1);
    }
    ssize_t cnt = usb_err;
    logd ("Done libusb_get_device_list cnt: %d", cnt);
    int idx = 0;
    int iusb_best_vendor_priority = 0;

    libusb_device * device;
    for (idx = 0; idx < cnt; idx ++) {                                  // For all USB devices...
      device = list [idx];
      struct usbvpid dev = iusb_vendor_get (device); 
      int vendor = dev.vendor;
      int product = dev.product;
      printf ("iusb_vendor_get vendor: 0x%04x  device: %p \n", vendor, device);
      if (vendor) {
        int vendor_priority = iusb_vendor_priority_get (vendor);
        //if (iusb_best_vendor_priority <  vendor_priority) {  // For first
        if (iusb_best_vendor_priority < vendor_priority) {  // For last
          iusb_best_vendor_priority = vendor_priority;
          iusb_best_vendor = vendor;
          iusb_best_product = product;
          iusb_best_device = device;
          strncpy (iusb_best_man, iusb_curr_man, sizeof (iusb_best_man));
          strncpy (iusb_best_pro, iusb_curr_pro, sizeof (iusb_best_pro));
        }
      }
    }

    
    if (iusb_best_vendor == 0 || iusb_best_device == NULL) {                                             // If no vendor...
      printf ("Error device not found iusb_best_vendor: 0x%04x  iusb_best_device: %p \n", iusb_best_vendor, iusb_best_device);
      libusb_free_device_list (list, 1);                                // Free device list now that we are finished with it
      return (-2);
    }
    printf ("Device found iusb_best_vendor: 0x%04x  iusb_best_device: 0x%04x  iusb_best_man: \"%s\"  iusb_best_pro: \"%s\" \n", iusb_best_vendor, iusb_best_device, iusb_best_man, iusb_best_pro);

    //usb_perms_set ();                                                 // Setup USB permissions, where needed

    if ((ep_in_addr == 255 && ep_out_addr == 0) || file_get ("/sdcard/hu_disable_selinux_chmod_bus")) {    // 
                                                                        // Disable SELinux and open /dev/bus permissions for SUsb
      int ret = system ("su -c \"setenforce 0 ; chmod -R 777 /dev/bus 1>/dev/null 2>/dev/null\""); // !! Binaries like ssd that write to stdout cause C system() to crash !
      logd ("iusb_usb_init system() w/ su ret: %d", ret);

      ret = system ("chmod -R 777 /dev/bus 1>/dev/null 2>/dev/null"); // !! Binaries like ssd that write to stdout cause C system() to crash !
      logd ("iusb_usb_init system() no su ret: %d", ret);
    }


    usb_err = libusb_open (iusb_best_device, & iusb_dev_hndl);
    logd ("libusb_open usb_err: %d (%s)  iusb_dev_hndl: %p  list: %p", usb_err, iusb_error_get (usb_err), iusb_dev_hndl, list);

    libusb_free_device_list (list, 1);                                  // Free device list now that we are finished with it

    if (usb_err != 0) {
      loge ("Error libusb_open usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
      return (-1);
    }
    logd ("Done libusb_open iusb_dev_hndl: %p", iusb_dev_hndl);

/*
    usb_err = libusb_set_auto_detach_kernel_driver (iusb_dev_hndl, 1);
    if (usb_err)
      loge ("Done libusb_set_auto_detach_kernel_driver usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    else
      logd ("Done libusb_set_auto_detach_kernel_driver usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
//*/
    usb_err = libusb_claim_interface (iusb_dev_hndl, 0);
    if (usb_err)
      loge ("Error libusb_claim_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));
    else
      logd ("OK libusb_claim_interface usb_err: %d (%s)", usb_err, iusb_error_get (usb_err));

    struct libusb_config_descriptor * config = NULL;
    usb_err = libusb_get_config_descriptor (iusb_best_device, 0, & config);
    if (usb_err != 0) {
      logd ("Expected Error libusb_get_config_descriptor usb_err: %d (%s)  errno: %d (%s)", usb_err, iusb_error_get (usb_err), errno, strerror (errno));    // !! ???? Normal error now ???
      //return (-1);

      if (ep_in_addr == 255) {// && ep_out_addr == 0)
        iusb_ep_in  = 129;                                  // Set  input endpoint
        iusb_ep_out =   2;                                  // Set output endpoint
        return (0);
      }

      iusb_ep_in  = ep_in_addr; //129;                                  // Set  input endpoint
      iusb_ep_out = ep_out_addr;//  2;                                  // Set output endpoint
      return (0);
    }

    //if (ep_in_addr == 255 && ep_out_addr == 0) {    // If USB forced


    int num_int = config->bNumInterfaces;                               // Get number of interfaces
    logd ("Done get_config_descriptor config: %p  num_int: %d", config, num_int);

    const struct libusb_interface            * inter;
    const struct libusb_interface_descriptor * interdesc;
    const struct libusb_endpoint_descriptor  * epdesc;

    for (idx = 0; idx < num_int; idx ++) {                              // For all interfaces...
      inter = & config->interface [idx];
      int num_altsetting = inter->num_altsetting;
      logd ("num_altsetting: %d", num_altsetting);
      int j = 0;
      for (j = 0; j < inter->num_altsetting; j ++) {                    // For all alternate settings...
        interdesc = & inter->altsetting [j];
        int num_int = interdesc->bInterfaceNumber;
        logd ("num_int: %d", num_int);
        int num_eps = interdesc->bNumEndpoints;
        logd ("num_eps: %d", num_eps);
        int k = 0;
        for (k = 0; k < num_eps; k ++) {                                // For all endpoints...
	        epdesc = & interdesc->endpoint [k];
          if (epdesc->bDescriptorType == LIBUSB_DT_ENDPOINT) {          // 5
            if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
              int ep_add = epdesc->bEndpointAddress;
              if (ep_add & LIBUSB_ENDPOINT_DIR_MASK) {
                if (iusb_ep_in < 0) {
                  iusb_ep_in = ep_add;                                   // Set input endpoint
                  logd ("iusb_ep_in: 0x%02x", iusb_ep_in);
                }
              }
              else {
                if (iusb_ep_out < 0) {
                  iusb_ep_out = ep_add;                                  // Set output endpoint
                  logd ("iusb_ep_out: 0x%02x", iusb_ep_out);
                }
              }
              if (iusb_ep_in > 0 && iusb_ep_out > 0) {                    // If we have both endpoints now...
                libusb_free_config_descriptor (config);

                if (ep_in_addr != 255 && (iusb_ep_in != ep_in_addr || iusb_ep_out != ep_out_addr)) {
                  logw ("MISMATCH Done endpoint search iusb_ep_in: 0x%02x  iusb_ep_out: 0x%02x  ep_in_addr: 0x%02x  ep_out_addr: 0x%02x", iusb_ep_in, iusb_ep_out, ep_in_addr, ep_out_addr);    // Favor libusb over passed in
                }
                else
                  logd ("Match Done endpoint search iusb_ep_in: 0x%02x  iusb_ep_out: 0x%02x  ep_in_addr: 0x%02x  ep_out_addr: 0x%02x", iusb_ep_in, iusb_ep_out, ep_in_addr, ep_out_addr);

                return (0);
              }
            }
          }
        }
      }
    }
                                                                        // Else if DON'T have both endpoints...
    loge ("Error in and/or out endpoints unknown iusb_ep_in: 0x%02x  iusb_ep_out: 0x%02x", iusb_ep_in, iusb_ep_out);
    libusb_free_config_descriptor (config);

    if (iusb_ep_in == -1)
      iusb_ep_in  = ep_in_addr; //129;                                  // Set  input endpoint
    if (iusb_ep_out == -1)
      iusb_ep_out = ep_out_addr;//  2;                                  // Set output endpoint

    if (iusb_ep_in == -1 || iusb_ep_out == -1)
      return (-1);

    loge ("!!!!! FIXED EP's !!!!!");
    return (0);
  }

  int hu_usb_stop () {
    iusb_state = hu_STATE_STOPPIN;
    logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
    int ret = iusb_deinit ();
    iusb_state = hu_STATE_STOPPED;
    logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
    return (ret);
  }


  int hu_usb_start (byte ep_in_addr, byte ep_out_addr) {
    int ret = 0;

    if (iusb_state == hu_STATE_STARTED) {
      logd ("CHECK: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
      return (0);
    }

    //#include <sys/prctl.h>
    //ret = prctl (PR_GET_DUMPABLE, 1, 0, 0, 0);  // 1 = SUID_DUMP_USER
    //logd ("prctl ret: %d", ret);

    iusb_state = hu_STATE_STARTIN;
    logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));

    iusb_best_vendor = 0;
    int tries = 0;
    while (iusb_best_vendor != USB_VID_GOO  && (iusb_best_product < 0x2d00 || iusb_best_product > 0x2d05) && tries ++ < 4) { //2000) {

      ret = iusb_init (ep_in_addr, ep_out_addr);
      if (ret < 0) {
        loge ("Error iusb_init");
        iusb_deinit ();
        iusb_state = hu_STATE_STOPPED;
        logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
        return (ret);
      }
      logd ("OK iusb_init");

	  printf("SHAI1 iusb_best_product id %d \n", iusb_best_product);

      if (iusb_best_vendor == USB_VID_GOO && (iusb_best_product >= 0x2d00 && iusb_best_product <= 0x2d05) ) {
//      if (iusb_best_vendor == USB_VID_GOO ) {
        logd ("Already OAP/AA mode, no need to call iusb_oap_start()");

        iusb_state = hu_STATE_STARTED;
        logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
        return (0);
      } 

      ret = iusb_oap_start ();
      if (ret < 0) {
        loge ("Error iusb_oap_start");
        iusb_deinit ();
        iusb_state = hu_STATE_STOPPED;
        logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
        return (-2);
      }
      logd ("OK iusb_oap_start");

      if (iusb_best_vendor != USB_VID_GOO && (iusb_best_product < 0x2d00 || iusb_best_product > 0x2d05)) {
        iusb_deinit ();
        ms_sleep (4000);//!!!!!!!!!!!!!!      (1000);                                                // 600 ms not enough; 700 seems OK
        logd ("Done iusb_best_vendor != USB_VID_GOO ms_sleep()");
      }
      else
        logd ("Done iusb_best_vendor == USB_VID_GOO");
    }

    if (iusb_best_vendor != USB_VID_GOO && (iusb_best_product < 0x2d00 || iusb_best_product > 0x2d05)) {
      loge ("No Google AA/Accessory mode iusb_best_vendor: 0x%x", iusb_best_vendor);
      iusb_deinit ();
      iusb_state = hu_STATE_STOPPED;
      logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
      return (-3);
    }

    iusb_state = hu_STATE_STARTED;
    logd ("  SET: iusb_state: %d (%s)", iusb_state, state_get (iusb_state));
    return (0);
  }

