#ifndef HU_USB_H
#define HU_USB_H

#include "hu_uti.h"

int hu_usb_recv  (byte * buf, int len, int tmo);                     // Used by hu_aap:hu_aap_usb_recv ()
int hu_usb_send  (byte * buf, int len, int tmo);                     // Used by hu_aap:hu_aap_usb_send ()
int hu_usb_stop  ();                                                 // Used by hu_aap:hu_aap_stop     ()
int hu_usb_start ();                // Used by hu_aap:hu_aap_start    ()
#endif

