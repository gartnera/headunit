#ifndef HU_UTI_H
#define HU_UTI_H

#ifndef DEBUG
  #define NDEBUG // Ensure debug stuff
#endif

#define hu_STATE_INITIAL   0
#define hu_STATE_STARTIN   1
#define hu_STATE_STARTED   2
#define hu_STATE_STOPPIN   3
#define hu_STATE_STOPPED   4

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <pthread.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <dirent.h>         // For opendir (), readdir (), closedir (), DIR, struct dirent.

// For possibly missing C functions
#include "c_utils.h"

#define RESULT_OK      0
#define RESULT_FAIL   -1

#define byte      uint8_t
#define DEFBUF    16384     //16384 65536  // Default buffer size is maximum for USB
#define DEF_BUF   512       // For Ascii strings and such

#define hu_LOG_EXT   1
#define hu_LOG_VER   2
#define hu_LOG_DEB   3
#define hu_LOG_WAR   5
#define hu_LOG_ERR   6

#ifdef NDEBUG

  #define  logx(...)
  #define  logv(...)
  #define  logd(...)
  #define  logw(...)
  #define  loge(...)

#else

  #define  logx(...)  hu_log(hu_LOG_EXT,LOGTAG,__func__,__VA_ARGS__)
  #define  logv(...)  hu_log(hu_LOG_VER,LOGTAG,__func__,__VA_ARGS__)
  #define  logd(...)  hu_log(hu_LOG_DEB,LOGTAG,__func__,__VA_ARGS__)
  #define  logw(...)  hu_log(hu_LOG_WAR,LOGTAG,__func__,__VA_ARGS__)
  #define  loge(...)  hu_log(hu_LOG_ERR,LOGTAG,__func__,__VA_ARGS__)

#endif

// Enables for hex_dump:
extern int ena_hd_hu_aad_dmp;
extern int ena_hd_tra_send;
extern int ena_hd_tra_recv;
extern int ena_log_aap_send;

extern int ena_log_extra;
extern int ena_log_verbo;

int hu_log (int prio, const char * tag, const char * func, const char * fmt, ...);

unsigned long ms_get();
unsigned long ms_sleep(unsigned long ms);
void hex_dump(char* prefix, int width, unsigned char* buf, int len);
char* vid_write_tail_buf_get  (int len);
char* vid_read_head_buf_get   (int* len);
char* aud_write_tail_buf_get  (int len);
char* aud_read_head_buf_get   (int* len);

extern int vid_buf_buf_tail;    // Tail is next index for writer to write to.   If head = tail, there is no info.
extern int vid_buf_buf_head;    // Head is next index for reader to read from.
extern int aud_buf_buf_tail;    // Tail is next index for writer to write to.   If head = tail, there is no info.
extern int aud_buf_buf_head;    // Head is next index for reader to read from.

// Android USB device priority:

//1d6b  Linux Foundation  PIDs:	0001  1.1 root hub  0002  2.0 root hub  -0003  3.0 root hub
//05c6  Qualcomm, Inc.

#define USB_VID_GOO 0x18D1    // The vendor ID should match Google's ID ( 0x18D1 ) and the product ID should be 0x2D00 or 0x2D01 if the device is already in accessory mode (case A).

#define USB_VID_HTC 0x0bb4
#define USB_VID_MOT 0x22b8
#define USB_VID_ACE 0x0502
#define USB_VID_HUA 0x12d1
#define USB_VID_PAN 0x10a9
#define USB_VID_ZTE 0x19d2
#define USB_VID_GAR 0x091e
#define USB_VID_XIA 0x2717
#define USB_VID_ASU 0x0b05
#define USB_VID_MEI 0x2a45
#define USB_VID_LEN 0x17ef


#define USB_VID_SAM 0x04e8
#define USB_VID_O1A 0xfff6  // Samsung ?

#define USB_VID_SON 0x0fce
#define USB_VID_LGE 0xfff5
#define USB_VID_LGD 0x1004

#define USB_VID_LIN 0x1d6b
#define USB_VID_QUA 0x05c6
#define USB_VID_ONE 0x2a70
#define USB_VID_COM 0x1519  // Comneon

#define USB_VID_ASE 0x0835  // Action Star Enterprise
#define USB_VID_OPO 0x22d9  // Oppo
#define USB_VID_LEE 0x2b0e  // Le Eco
#define USB_VID_ZUK 0x2b4c  // ZUK
#define USB_VID_BB  0x0fca  // Blackberry
#define USB_VID_BQ  0x2a47	// BQ

//#define USB_PID_OM8 0x0f87
//#define USB_PID_MOG 0x2e76


#define USB_PID_ACC_MIN       0x2D00
#define USB_PID_ACC_MAX       0x2D05

//#define USB_PID_ACC         0x2D00      // Accessory                  100
//#define USB_PID_ACC_ADB     0x2D01      // Accessory + ADB            110
//#define USB_PID_AUD         0x2D02      //                   Audio    001
//#define USB_PID_AUD_ADB     0x2D03      //             ADB + Audio    011
//#define USB_PID_ACC_AUD     0x2D04      // Accessory       + Audio    101
//#define USB_PID_ACC_AUD_ADB 0x2D05      // Accessory + ADB + Audio    111
// No 000 (Nothing) or 010 (adb only, which is default)


//#endif  //#ifndef UTILS_INCLUDED

#endif
