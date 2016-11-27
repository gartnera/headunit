#include "hu_usb.h"

#define LOGTAG "hu_usb"
#include "hu_uti.h" // Utilities
#include "hu_oap.h" // Open Accessory Protocol

int iusb_state = 0; // 0: Initial    1: Startin    2: Started    3: Stoppin    4: Stopped

#include <stdbool.h>
#include <libusb.h>

#ifndef LIBUSB_LOG_LEVEL_NONE
#define LIBUSB_LOG_LEVEL_NONE 0
#endif
#ifndef LIBUSB_LOG_LEVEL_ERROR
#define LIBUSB_LOG_LEVEL_ERROR 1
#endif
#ifndef LIBUSB_LOG_LEVEL_WARNING
#define LIBUSB_LOG_LEVEL_WARNING 2
#endif
#ifndef LIBUSB_LOG_LEVEL_INFO
#define LIBUSB_LOG_LEVEL_INFO 3
#endif
#ifndef LIBUSB_LOG_LEVEL_DEBUG
#define LIBUSB_LOG_LEVEL_DEBUG 4
#endif

struct libusb_device_handle *iusb_dev_hndl = NULL;
libusb_device *iusb_best_device = NULL;
int iusb_ep_in = -1;
int iusb_ep_out = -1;
int iusb_best_vendor = 0;
int iusb_best_product = 0;

#ifndef NDEBUG

static char *iusb_error_get(int error)
{
    switch (error)
    {
    case LIBUSB_SUCCESS: // 0
        return ("LIBUSB_SUCCESS");
    case LIBUSB_ERROR_IO: // -1
        return ("LIBUSB_ERROR_IO");
    case LIBUSB_ERROR_INVALID_PARAM: // -2
        return ("LIBUSB_ERROR_INVALID_PARAM");
    case LIBUSB_ERROR_ACCESS: // -3
        return ("LIBUSB_ERROR_ACCESS");
    case LIBUSB_ERROR_NO_DEVICE: // -4
        return ("LIBUSB_ERROR_NO_DEVICE");
    case LIBUSB_ERROR_NOT_FOUND: // -5
        return ("LIBUSB_ERROR_NOT_FOUND");
    case LIBUSB_ERROR_BUSY: // -6
        return ("LIBUSB_ERROR_BUSY");
    case LIBUSB_ERROR_TIMEOUT: // -7
        return ("LIBUSB_ERROR_TIMEOUT");
    case LIBUSB_ERROR_OVERFLOW: // -8
        return ("LIBUSB_ERROR_OVERFLOW");
    case LIBUSB_ERROR_PIPE: // -9
        return ("LIBUSB_ERROR_PIPE");
    case LIBUSB_ERROR_INTERRUPTED: // -10
        return ("Error:LIBUSB_ERROR_INTERRUPTED");
    case LIBUSB_ERROR_NO_MEM: // -11
        return ("LIBUSB_ERROR_NO_MEM");
    case LIBUSB_ERROR_NOT_SUPPORTED: // -12
        return ("LIBUSB_ERROR_NOT_SUPPORTED");
    case LIBUSB_ERROR_OTHER: // -99
        return ("LIBUSB_ERROR_OTHER");
    }
    return ("LIBUSB_ERROR Unknown error"); //: %d", error);
}

#endif

static int iusb_bulk_transfer(int ep, byte *buf, int len, int tmo)
{ // 0 = unlimited timeout

    char *dir = "recv";
    if (ep == iusb_ep_out)
    {
        dir = "send";
    }

    if (iusb_state != hu_STATE_STARTED)
    {
        logd("CHECK: iusb_bulk_transfer start iusb_state: %d (%s) dir: %s  ep: 0x%02x  buf: %p  len: %d  tmo: %d", iusb_state, state_get(iusb_state), dir, ep, buf, len, tmo);
        return RESULT_FAIL;
    }

#define MAX_LEN 65536

    if (ep == iusb_ep_in && len > MAX_LEN)
    {
        len = MAX_LEN;
    }

    if (ena_log_extra)
    {
        logw("Start dir: %s  ep: 0x%02x  buf: %p  len: %d  tmo: %d", dir, ep, buf, len, tmo);
    }

#ifdef DEBUG
    if (ena_hd_tra_send && ep == iusb_ep_out)
        hex_dump("US: ", 16, buf, len);
#endif

    int usb_err = -2;
    int total_bytes_xfrd = 0;
    int bytes_xfrd = 0;
    // Check the transferred parameter for bulk writes. Not all of the data may have been written.

    // Check transferred when dealing with a timeout error code.
    // libusb may have to split your transfer into a number of chunks to satisfy underlying O/S requirements, meaning that the timeout may expire after the first few chunks have completed.
    // libusb is careful not to lose any data that may have been transferred; do not assume that timeout conditions indicate a complete lack of I/O.

    errno = 0;
    int continue_transfer = 1;
    while (continue_transfer)
    {
        bytes_xfrd = 0; // Default tranferred = 0

        unsigned long ms_start = ms_get();
        usb_err = libusb_bulk_transfer(iusb_dev_hndl, ep, buf, len, &bytes_xfrd, tmo);
        unsigned long ms_duration = ms_get() - ms_start;

        if (ms_duration > 400)
        {
            loge("ms_duration: %d dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", ms_duration, dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get(usb_err), errno, strerror(errno));
        }

        continue_transfer = 0; // Default = transfer done

        if (bytes_xfrd > 0)
        {                                   // If bytes were transferred
            total_bytes_xfrd += bytes_xfrd; // Add to total
        }

        if ((total_bytes_xfrd > 0 || bytes_xfrd > 0) && usb_err == LIBUSB_ERROR_TIMEOUT)
        { // If bytes were transferred but we had a timeout...
            logd("CONTINUE dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get(usb_err));
            buf += bytes_xfrd; // For next transfer, point deeper info buf
            len -= bytes_xfrd; // For next transfer, reduce buf len capacity
        }
        else if (usb_err < 0 && usb_err != LIBUSB_ERROR_TIMEOUT)
        {
            loge("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get(usb_err), errno, strerror(errno));
        }
        else if (ena_log_verbo && usb_err != LIBUSB_ERROR_TIMEOUT)
        {
            logd("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get(usb_err), errno, strerror(errno));
        }
        else if (ena_log_extra)
        {
            logw("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get(usb_err), errno, strerror(errno));
        }
    }

    if (total_bytes_xfrd <= 0 && usb_err < 0)
    {
        if (errno == EAGAIN || errno == ETIMEDOUT || usb_err == LIBUSB_ERROR_TIMEOUT)
        {
            return RESULT_OK;
        }

        loge("Done dir: %s  len: %d  bytes_xfrd: %d  total_bytes_xfrd: %d  usb_err: %d (%s)  errno: %d (%s)", dir, len, bytes_xfrd, total_bytes_xfrd, usb_err, iusb_error_get(usb_err), errno, strerror(errno));
        hu_usb_stop(); // Other errors here are fatal, so stop USB
        return RESULT_FAIL;
    }

#ifndef NDEBUG
    if (ena_hd_tra_recv && ep == iusb_ep_in)
    {
        hex_dump("UR: ", 16, buf, total_bytes_xfrd);
    }
#endif

    return (total_bytes_xfrd);
}

int hu_usb_recv(byte *buf, int len, int tmo)
{
    int ret = iusb_bulk_transfer(iusb_ep_in, buf, len, tmo); // milli-second timeout
    return ret;
}

int hu_usb_send(byte *buf, int len, int tmo)
{
    int ret = iusb_bulk_transfer(iusb_ep_out, buf, len, tmo); // milli-second timeout
    return ret;
}

static int iusb_control_transfer(libusb_device_handle *usb_hndl, uint8_t req_type, uint8_t req_val, uint16_t val, uint16_t idx, byte *buf, uint16_t len, unsigned int tmo)
{
    logd("Start usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_hndl, req_type, req_val, val, idx, buf, len, tmo);

    int usb_err = libusb_control_transfer(usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
    if (usb_err < 0)
    {
        loge("Error usb_err: %d (%s)  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, iusb_error_get(usb_err), usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
        return RESULT_FAIL;
    }

    logd("Done usb_err: %d  usb_hndl: %p  req_type: %d  req_val: %d  val: %d  idx: %d  buf: %p  len: %d  tmo: %d", usb_err, usb_hndl, req_type, req_val, val, idx, buf, len, tmo);
    return RESULT_OK;
}

static int iusb_oap_start(libusb_device *device)
{
    // Open the device, send it OAP initialize command
    int result;
    struct libusb_device_handle *device_handle;
    result = libusb_open(device, &device_handle);
    if (result < 0)
    {
        loge("Failed to open USB device!");
        return RESULT_FAIL;
    }

    uint8_t req_type = USB_SETUP_HOST_TO_DEVICE | USB_SETUP_TYPE_VENDOR | USB_SETUP_RECIPIENT_DEVICE;
    uint8_t req_val = ACC_REQ_SEND_STRING;
    unsigned int tmo = 1000;

    iusb_control_transfer(device_handle, req_type, req_val, 0, ACC_IDX_MAN, (uint8_t*)AAP_VAL_MAN, strlen(AAP_VAL_MAN) + 1, tmo);
    iusb_control_transfer(device_handle, req_type, req_val, 0, ACC_IDX_MOD, (uint8_t*)AAP_VAL_MOD, strlen(AAP_VAL_MOD) + 1, tmo);

    logd("Accessory IDs sent");
    req_val = ACC_REQ_START;
    if (iusb_control_transfer(device_handle, req_type, req_val, 0, 0, NULL, 0, tmo) < 0)
    {
        loge("Error Accessory mode start request sent");
        return RESULT_FAIL;
    }

    logd("OK Accessory mode start request sent");
    libusb_close(device_handle);

    return RESULT_OK;
}

static int iusb_deinit()
{ // !!!! Need to better reset and wait a while to kill transfers in progress and auto-restart properly
    if (iusb_dev_hndl == NULL)
    {
        loge("Done iusb_dev_hndl: %p", iusb_dev_hndl);
        return RESULT_FAIL;
    }

    int usb_err = libusb_release_interface(iusb_dev_hndl, 0); // Can get a crash inside libusb_release_interface()
    if (usb_err != 0)
    {
        loge("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get(usb_err));
    }
    else
    {
        logd("Done libusb_release_interface usb_err: %d (%s)", usb_err, iusb_error_get(usb_err));
    }

    libusb_reset_device(iusb_dev_hndl);

    libusb_close(iusb_dev_hndl);
    logd("Done libusb_close");
    iusb_dev_hndl = NULL;
    libusb_exit(NULL); // Put here or can get a crash from pulling cable
    return RESULT_OK;
}

static int iusb_vendor_priority_get(int vendor)
{
    switch (vendor)
    {
        case USB_VID_GOO:
            return 10;
        case USB_VID_HTC:
            return 9;
        case USB_VID_MOT:
            return 8;
        case USB_VID_SAM:
            return 4;
        case USB_VID_SON:
            return 3;
        case USB_VID_LGE:
        case USB_VID_LGD:
        case USB_VID_ACE:
        case USB_VID_HUA:
        case USB_VID_PAN:
        case USB_VID_ZTE:
        case USB_VID_GAR:
        case USB_VID_O1A:
        case USB_VID_QUA:
        case USB_VID_ONE:
        case USB_VID_XIA:
        case USB_VID_ASU:
        case USB_VID_MEI:
        case USB_VID_LEN:
        case USB_VID_OPO:
        case USB_VID_LEE:
        case USB_VID_ZUK:
        case USB_VID_BB:
        case USB_VID_BQ:
            return 5;
        default:
            return 0;
    }
}

int hu_usb_stop()
{
    iusb_state = hu_STATE_STOPPIN;
    logd("  SET: iusb_state: %d (%s)", iusb_state, state_get(iusb_state));
    int ret = iusb_deinit();
    iusb_state = hu_STATE_STOPPED;
    logd("  SET: iusb_state: %d (%s)", iusb_state, state_get(iusb_state));
    return (ret);
}

/**
* Queries device for OAP support as standard requests and returns true if it works.
*/
static int device_supports_accessory_mode(libusb_device *device)
{
    int result = false;
    struct libusb_device_handle *device_handle;
    result = libusb_open(device, &device_handle);
    if (result < 0)
    {
        logw("Device 0x%04x doesn't support accessory mode, skipping...");
        return false;
    }

    uint8_t version_data[2] = {0, 0};
    uint8_t req_type = USB_SETUP_DEVICE_TO_HOST | USB_SETUP_TYPE_VENDOR | USB_SETUP_RECIPIENT_DEVICE;
    uint8_t req_val = ACC_REQ_GET_PROTOCOL; // 52 == get protocol request

    result = iusb_control_transfer(device_handle, req_type, req_val, 0, 0, version_data, sizeof(version_data), 100);
    libusb_close(device_handle);

    if (result != 0)
    {
        logw("Device 0x%04x doesn't support accessory mode, skipping...'");
        return false;
    }

    int oap_protocol_level = version_data[1] << 8 | version_data[0];
    if (oap_protocol_level < 2)
    {
        logw("Device 0x%04x doesn't support OAP protocol level 2, skipping...");
        return false;
    }

    return true;
}

/**
* Looks for a phone / device with best priority on USB bus and switch it into OAP mode.
*/
static int switch_device_to_oap_mode()
{
    int result;
    libusb_device **list;
    int device_count = libusb_get_device_list(NULL, &list);
    if (device_count < 0)
    {
        loge("Failed to retrieve device list for OAC mode - %d (%s).", result, iusb_error_get(result));
        return RESULT_FAIL;
    }

    libusb_device *best_device = NULL;
    uint16_t best_vendor = 0;
    uint16_t best_product = 0;
    int best_device_priority = 0;

    // Enumerate connected devices and try to find a phone.
    int i;
    for (i = 0; i < device_count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor descriptor;
        result = libusb_get_device_descriptor(device, &descriptor);
        if (result < 0)
            continue;

        int device_priority = iusb_vendor_priority_get(descriptor.idVendor);

        // If we identified a compatible vendor, we still need to check if the device is
        // actually a phone - otherwise we can start talking to a Samsung USB stick or something.
        if (device_priority > 0 && !device_supports_accessory_mode(device))
        {
            // Nope, device doesn't support OAP, skip it.
            device_priority = 0;
        }

        if (best_device_priority < device_priority)
        {
            best_vendor = descriptor.idVendor;
            best_product = descriptor.idProduct;
            best_device_priority = device_priority;
            best_device = device;
        }
    }

    // Keep the reference to our best device so free_device_list won't
    // close it.
    if (best_device) {
        libusb_ref_device(best_device);
    }

    libusb_free_device_list(list, 1);

    // We should have the best vendor now.
    if (best_vendor == 0 && best_product == 0)
    {
        // No device found.
        loge("No Android device ready for Android Auto has been found on USB.");
        return RESULT_FAIL;
    }

    // Check if device is already in OAC mode
    if (best_vendor == USB_VID_GOO && best_product >= 0x2d00 && best_product < 0x2d05)
    {
        logd("Found device already in OAC mode, skipping switch...");
        return RESULT_OK;
    }

    logd("Switching device 0x%04x / 0x%04x into accessory mode...", best_vendor, best_product);
    result = iusb_oap_start(best_device);
    libusb_unref_device(best_device);

    if (result < 0) {
        loge("Failed to switch device into OAP mode!");
        return RESULT_FAIL;
    }

    return RESULT_OK;
}

/**
* Enumerates USB devices and returns true if there is a USB device in Android Accessory
* mode present.
* Returns <0 if none available, or vendor productId if the device is here.
*/
static int is_device_in_oap_mode_present()
{
    int result;

    libusb_device **list;
    int device_count = libusb_get_device_list(NULL, &list);
    if (device_count < 0)
    {
        loge("Failed to retrieve device list - %d (%s).", result, iusb_error_get(result));
        return RESULT_FAIL;
    }

    logd("Got %d USB devices.", device_count);

    // Enumerate devices and check if one with Google ID exists.
    int product_id = RESULT_FAIL;

    int i;
    for (i = 0; i < device_count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor descriptor;
        result = libusb_get_device_descriptor(device, &descriptor);
        if (result < 0)
            continue;

        if (descriptor.idVendor == USB_VID_GOO && (descriptor.idProduct >= 0x2d00 && descriptor.idProduct < 0x2d05))
        {
            logd("Device in OAC mode found!");
            product_id = descriptor.idProduct;
            break;
        }
    }

    libusb_free_device_list(list, 1);
    return product_id;
}

int hu_usb_start()
{
    int result;

    if (iusb_state == hu_STATE_STARTED)
    {
        logd("USB already started.");
        return RESULT_OK;
    }

    iusb_state = hu_STATE_STARTIN;
    logd("Starting USB....");

    // Setup libUSB library
    result = libusb_init(NULL);
    if (result < 0)
    {
        loge("Failed to init libusb, error: %d (%s)", result, iusb_error_get(result));
        return -1;
    }

    libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_INFO);

    // Ok, the library is set up now. Check if we have a device connected in OAC mode
    // and attempt to kick it into that mode if not.
    int product_id = is_device_in_oap_mode_present();

    int retry_count = 60;
    while (product_id < 0) {
        result = switch_device_to_oap_mode();
        ms_sleep(50);

        if (result == RESULT_OK) {
            int wait_count_timeout = 20;        // Wait for 10 secs max for device to come back in OAP mode
            while ((product_id = is_device_in_oap_mode_present()) < 0) {
                logd("Waiting for device in OAP mode...");
                ms_sleep(500);
            }
        }

        if (product_id < 0) {
            retry_count--;
            ms_sleep(1000);
        }
    }

    logd("Got a device in OAP mode - acquiring...");
    iusb_dev_hndl = libusb_open_device_with_vid_pid(NULL, USB_VID_GOO, product_id);
    if (iusb_dev_hndl == NULL)
    {
        loge("Couldn't open the OAP USB device!");
        return RESULT_FAIL;
    }

    result = libusb_claim_interface(iusb_dev_hndl, 0);
    if (result < 0)
    {
        loge("Failed to claim device: %d.", result);
        return RESULT_FAIL;
    }

    iusb_best_device = libusb_get_device(iusb_dev_hndl);
    if (iusb_best_device == NULL)
    {
        loge("Couldn't get acquired device!");
        return RESULT_FAIL;
    }

    // We have claimed the USB device, now get the USB endpoint addresses
    struct libusb_config_descriptor *config = NULL;
    result = libusb_get_config_descriptor(iusb_best_device, 0, &config);
    if (result)
    {
        loge(" Failed to retrieve libusb config descriptor!");
        return RESULT_FAIL;
    }

    int num_int = config->bNumInterfaces;
    const struct libusb_interface *interface = NULL;

    int i;
    for (i = 0; i < num_int; i++)
    {
        interface = &config->interface[i];
        int num_altsetting = interface->num_altsetting;

        int j;
        for (j = 0; j < num_altsetting; j++)
        {
            const struct libusb_interface_descriptor *interface_descriptor = NULL;
            interface_descriptor = &interface->altsetting[j];
            int num_desc_int = interface_descriptor->bInterfaceNumber;
            int num_desc_ep = interface_descriptor->bNumEndpoints;

            int k;
            for (k = 0; k < num_desc_ep; k++)
            {
                const struct libusb_endpoint_descriptor *endpoint_descriptor = NULL;
                endpoint_descriptor = &interface_descriptor->endpoint[k];

                // Check if we have proper type of endpoint and then remember it
                if (endpoint_descriptor->bDescriptorType == LIBUSB_DT_ENDPOINT)
                {
                    if ((endpoint_descriptor->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK)
                    {
                        int endpoint_address = endpoint_descriptor->bEndpointAddress;
                        if (endpoint_address & LIBUSB_ENDPOINT_DIR_MASK)
                        {
                            if (iusb_ep_in < 0)
                            {
                                iusb_ep_in = endpoint_address;
                                logd("Got input endpoint: 0x%02x", iusb_ep_in);
                            }
                        }
                        else
                        {
                            if (iusb_ep_out < 0)
                            {
                                iusb_ep_out = endpoint_address;
                                logd("Got output endpoint: 0x%02x", iusb_ep_in);
                            }
                        }
                    }
                }

                // If we have both enpoints, stop iterating and continue
                if (iusb_ep_in > 0 && iusb_ep_out > 0)
                {
                    logd("Got both endpoints, continuing...");
                    libusb_free_config_descriptor(config);
                    iusb_state = hu_STATE_STARTED;
                    return RESULT_OK;
                }
            }
        }
    }

    loge("Failed to acquire endpoints!");
    iusb_state = hu_STATE_STOPPED;
    return RESULT_FAIL;
}
