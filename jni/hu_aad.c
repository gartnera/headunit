  // Android Auto protocol Dump API:
  //    unsigned int hu_aad_dmp (unsigned char * prefix, unsigned char * src, int chan, int flags, unsigned char * buf, int len);


  #define LOGTAG "hu_aad"
  #include "hu_uti.h"

  #include "hu_aap.h"

#ifndef NDEBUG

  // AA Dump Internal:

/* Android Auto protocol:

  Prefix:
    1 byte channel / service ID
    1 byte flags (first, last, encrypted, ?)
    2 bytes body length

  Body:
    First 2 bytes are Message Type msg_type
    Remaining bytes are protobufs data, usually starting with 0x08 for integer or 0x0a for array

  Startup:
    USB  initiated by HU. USB host mode detect phone. Switch AA device to Android Accessory mode:
    Wifi initiated by AA. NFC trigger TCP connect to AA port/IP discovered by Wifip2p. TCP accept():

------------------------------------------------------------
Control Channel 0:

    HU sends msg_type:

    HU -> 1: Version Request

    AA -> 2: Version Response
    HU -> 3: SSL Handshake Data

    AA -> 3: SSL Handshake Data
    HU -> 4: SSL Authentication Complete Notification

    AA -> 5: Service Discovery Request
    HU -> 6: Service Discovery Response


    Before Audio Sink Media Setup below, buf after Channel Opens: (here because Control Channel 0)
      AA -> 18: Audio Focus Request
      HU -> 19: Audio Focus Notification            with data:

-----------------------------------------------------------
Channel specified for each service:

    For each service discovered:
      AA -> 7: Channel Open Request
      HU -> 8: Channel Open Response

    To start OK, AA requires:
      HU -> 32771: Sensor Notification        with data: "Driving Status = Stopped"

    For each media sink:
      AA -> 32768: Media Setup Request
      HU -> 32771: Media Setup Response
    Video sink adds:
      HU -> 32776: VideoFocus Notification   with data: ?

    Touch/Input/Keys/Audio:
      AA -> 32770: Touch/Input/Audio Start/Stop Request
      HU -> 32771: Key Binding/Audio Response

    Video Sink:
      AA -> 32769: Sen/Med Start req
      HU -> 32771: Media Setup rsp
    Add:
      HU -> 32776: VideoFocus Notification        !!!! Duplicate ??


    Video starts with:
      AA -> 1: Codec Config Request
      HU -> 32772: Video Ack Response
    Then regular:
      AA -> 0: Media Data
      HU -> 32772: Video Ack Response


*/

  char * iaad_msg_type_str_get (int msg_type, unsigned char * src, int len) {   // Source:  HU = HU > AA   AA = AA > HU

    #define k3 32768+                                                   // Service specific message types start at 0x8000

    switch (msg_type) {
      case 0:   return ("Media Data");
      case 1:   if       (* src == 'H')  return ("Version Request");    // Start AA Protocol
                else if  (* src == 'A')  return ("Codec Data");         // First Video packet, respond with Media Ack
                else                     return ("1 !");
      case 2:   return ("Version Response");                            // short:major  short:minor   short:status
      case 3:   return ("SSL Handshake Data");                          // First/Request from HU, Second/Response from AA
        //case 5123:return ("SSL Change Cipher Spec");                      // 0x1403
        //case 5379:return ("SSL Alert");                                   // 0x1503
        //case 5635:return ("SSL Handshake");                               // 0x1603
        //case 5891:return ("SSL App Data");                                // 0x1703
      case 4:   return ("SSL Authentication Complete Notification");
      case 5:   return ("Service Discovery Request");
      case 6:   return ("Service Discovery Response");
      case 7:   return ("Channel Open Request");
      case 8:   return ("Channel Open Response");                       // byte:status
      case 9:   return ("9 ??");
      case 10:  return ("10 ??");
      case 11:  return ("Ping Request");
      case 12:  return ("Ping Response");
      case 13:  return ("Navigation Focus Request");
      case 14:  return ("Navigation Focus Notification");               // NavFocusType
      case 15:  return ("Byebye Request");
      case 16:  return ("Byebye Response");
      case 17:  return ("Voice Session Notification");
      case 18:  return ("Audio Focus Request");
      case 19:  return ("Audio Focus Notification");                    // AudioFocusType   (AudioStreamType ?)

      case k3 0: return ("Media Setup Request");                        // Video and Audio sinks receive this and send k3 3 / 32771
      case k3 1: if       (* src == 'H') return ("Touch Notification");
                 else if  (* src == 'A') return ("Sensor/Media Start Request");
                 else                    return ("32769 !");            // src AA also Media Start Request ????
      case k3 2: if       (* src == 'H') return ("Sensor Start Response");
                 else if  (* src == 'A') return ("Touch/Input/Audio Start/Stop Request");
                 else                    return ("32770 !");            // src AA also Media Stop Request ?
      case k3 3: if       (len  == 6)   return ("Media Setup Response");
                 else if  (len  == 2)   return ("Key Binding Response");
                 else                   return ("Sensor Notification");
      case k3 4: return ("Codec/Media Data Ack");
      case k3 5: return ("Mic Start/Stop Request");
      case k3 6: return ("k3 6 ?");
      case k3 7: return ("Media Video ? Request");
      case k3 8: return ("Video Focus Notification");

      case 65535:return ("Framing Error Notification");

    }
    return ("Unknown");
  }

  void iaad_adj (unsigned int * rmv, unsigned char ** buf, unsigned int * lft, unsigned int adj) {
    * buf += adj;
    * rmv += adj;
    * lft -= adj;
  }

  unsigned int iaad_vint_dec (int log_dmp, unsigned char * buf, unsigned int len, unsigned long * vintp) {  // Varint decode
    unsigned int rmv = 0, adj = 0;
    unsigned int lft = len;
    unsigned long vint = 0;

    adj = 1;
    while (rmv < 10 && lft > 0) {  // Assumed unsigned, 9 bytes = 63 bits
      unsigned char new_byte = buf [0];

      vint |= ((new_byte & 0x7f) << (7 * rmv));
      iaad_adj (& rmv, & buf, & lft, adj);

      if ((new_byte & 0x80) == 0)
        break;
    }
    if (rmv > 9) {
      if (log_dmp)
        loge ("rmv > 9");
      rmv = 0xffffffff;
    }

    if (vintp)
      * vintp = vint;
    return (rmv);
  }

  unsigned int iaad_dmp_vint (int log_dmp, unsigned int n, unsigned char * buf, unsigned int len) {
    unsigned int rmv = 0, adj = 0;
    unsigned int lft = len;
    unsigned long vint = 0;

    unsigned int num = buf [0] >> 3;
    adj = 1;
    iaad_adj (& rmv, & buf, & lft, adj);

    adj = iaad_vint_dec (log_dmp, buf, lft, & vint);
    iaad_adj (& rmv, & buf, & lft, adj);

    if (log_dmp) {
      //logd ("n: %1.1d  num: %1.1d  vint: %20ld", n, num, vint);
      //logd ("%1.1d  %1.1d  %6ld", n, num, vint);

      unsigned char str_buf [256] = {0};
      //snprintf (str_buf, sizeof (str_buf), "%1.1d", num);
      int ctr = 0;
      for (ctr = 0; ctr < n - 1; ctr ++)
        strncat (str_buf, "                    ", sizeof (str_buf));


      //unsigned char str_buf2 [256] = {0};
      //snprintf (str_buf2, sizeof (str_buf), "%s%1.1d", str_buf, num);
      logd ("%s%1.1d %4x %5ld", str_buf, num, vint, vint);
    }
    return (rmv);
  }

  unsigned int iaad_dmp_n (int log_dmp, unsigned int n, unsigned char * buf, unsigned int len);

  unsigned int iaad_dmp_arry (int log_dmp, unsigned int n, unsigned char * buf, unsigned int len) {
    if (n > 8) {
      //if (log_dmp)
        loge ("iaad_dmp_arry n: %d", n);
    }
    unsigned int rmv = 0, adj = 0;
    unsigned int lft = len;
    unsigned int num = buf [0] >> 3;
    adj = 1;
    iaad_adj (& rmv, & buf, & lft, adj);

    unsigned long alen = 0;
    adj = iaad_vint_dec (log_dmp, buf, lft, & alen);                    // Get length of array
    if (adj > 8192 || alen > 8192) {
      if (log_dmp)
        loge ("iaad_dmp_arry iaad_vint_dec adj: %d  alen: %ld", adj, alen);
      return (0xffffffff);
    }
    //logd ("iaad_dmp_arry iaad_vint_dec adj: %d  alen: %ld", adj, alen);
    iaad_adj (& rmv, & buf, & lft, adj);                                // Point to array

    if (alen == 0)
      return (rmv);
    if (alen < 0) {
      if (log_dmp)
        loge ("iaad_dmp_arry alen: %ld", alen);
      return (rmv);
    }

    if (log_dmp) {
      //logd ("iaad_dmp_arry n: %d  num: %d  alen: %ld", n, num, alen); // Dump raw array
      unsigned char str_buf [256] = {0};
      int ctr = 0;
      for (ctr = 0; ctr < n - 1; ctr ++)
        strncat (str_buf, "                    ", sizeof (str_buf));

      unsigned char str_buf2 [256] = {0};
      snprintf (str_buf2, sizeof (str_buf), "%s%1.1d", str_buf, num);   // Dump raw array
      hex_dump (str_buf2, 16, buf, alen);
    }

    adj = iaad_dmp_n (0, n + 1, buf, alen);                             // Try first without error logging to see if this is a valid array
    if (adj != alen || adj > 8192) {                                    //  "|| rmv + adj != len) {" only works if array is last item in packet
      //logd ("Array not embedded data adj: %d  alen: %ld  rmv: %d  len: %d", adj, alen, rmv, len);
      return (rmv + alen);
    }

    adj = iaad_dmp_n (log_dmp, n + 1, buf, alen);
    /*if (adj > 8192) {
      if (log_dmp)
        loge ("iaad_dmp_arry iaad_dmp_arry adj: %d  alen: %ld", adj, alen);
      return (0xffffffff);
    }*/
    iaad_adj (& rmv, & buf, & lft, adj);

    return (rmv);
  }

  unsigned int iaad_dmp_n (int log_dmp, unsigned int n, unsigned char * buf, unsigned int len) {
    unsigned int rmv = 0, adj = 0;
    unsigned int lft = len;

    while (lft > 0) {                                                   // While bytes left to process...
      //loge ("!!!! 1  n: %d  adj: %d  len: %d  lft: %d  rmv: %d  buf: %p", n, adj, len, lft, rmv, buf);
      if ((((unsigned int) buf [0]) & 0x07) == 0) {                     // If varint...
        adj = iaad_dmp_vint (log_dmp, n, buf, lft);                     // Adjust past the varint
      }

      else if ((((unsigned int) buf [0]) & 0x07) == 2) {                // If array...
        adj = iaad_dmp_arry (log_dmp, n, buf, lft);                     // Adjust past the array
      }

      else {                                                            // Else if unknown...
        if (log_dmp)
          loge ("iaad_dmp_n buf[0]: 0x%x", buf [0]);
        return (rmv);
      }

      iaad_adj (& rmv, & buf, & lft, adj);                              // Apply adjustment
      //loge ("!!!! 2  n: %d  adj: %d  len: %d  lft: %d  rmv: %d  buf: %p", n, adj, len, lft, rmv, buf);
    }

    if (lft != 0 || rmv != len || rmv < 0)
      if (log_dmp)
        loge ("iaad_dmp_n after content len: %d  lft: %d  rmv: %d  buf: %p", len, lft, rmv, buf);
    return (rmv);
  }

  char * msg_type_enc_get (int msg_type) {

    switch (msg_type) {
      case 5123:  return ("SSL Change Cipher Spec");                    // 0x1403
      case 5379:  return ("SSL Alert");                                 // 0x1503
      case 5635:  return ("SSL Handshake");                             // 0x1603
      case 5891:  return ("SSL App Data");                              // 0x1703
    }

    return (NULL);
  }


      // Android Auto protocol dump:

  unsigned int hu_aad_dmp (unsigned char * prefix, unsigned char * src, int chan, int flags, unsigned char * buf, int len) {   // Source:  HU = HU > AA   AA = AA > HU

    unsigned int rmv = 0;
    unsigned int adj = 0;
    unsigned int lft = len;

    if (len < 2) {                                                      // If less than 2 bytes, needed for msg_type...
      loge ("hu_aad_dmp len: %d", len);
      return (rmv);                                                     // Done with error
    }

/*
    int checksum_vid = 0;
    int checksum_all = 0;
    int ctr = 0;
    for (ctr = 0; ctr < len; ctr ++) {                                 // For all bytes...
      checksum_all += (int) buf [ctr];                                 // Checksum from offset 0x00 to end
      if (ctr >= 0x10)
        checksum_vid += (int) buf [ctr];                               // Checksum from offset 0x10 to end
    }
    logd ("checksum_vid: %10d  checksum_all: %10d", checksum_vid, checksum_all);
*/
    unsigned int msg_type = (((unsigned int) buf [0]) << 8) + ((unsigned int) buf [1]);

    int is_media = 0;
    if (chan == AA_CH_VID || chan == AA_CH_MIC || chan == AA_CH_AUD || chan == AA_CH_AU1 || chan == AA_CH_AU2)
      is_media = 1;

    if (is_media && (flags == 8 || flags == 0x0a || msg_type == 0)) // || msg_type ==1)    // Ignore Video/Audio Data
      return (rmv);

    if (is_media && msg_type == 32768 + 4)                                                 // Ignore Video/Audio Ack
      return (rmv);

    adj = 2;                                                            // msg_type = 2 bytes
    iaad_adj (& rmv, & buf, & lft, adj);                                // Adjust past the msg_type to content

    char * msg_type_str = iaad_msg_type_str_get (msg_type, src, lft);   // Get msg_type string
    if (flags == 0x08)
      logd ("%s src: %s  lft: %5d  Media Data Mid", prefix, src, lft);
    else if (flags == 0x0a)
      logd ("%s src: %s  lft: %5d  Media Data End", prefix, src, lft);
    else
      logd ("%s src: %s  lft: %5d  msg_type: %5d %s", prefix, src, lft, msg_type, msg_type_str);

    if (ena_hd_hu_aad_dmp)                                              // If hex dump enabled...
      hex_dump (prefix, 16, buf, lft);                                  // Hexdump

    if (flags == 0x08)                                                  // If Media Data Mid...
      return (len);                                                     // Done

    if (flags == 0x0a)                                                  // If Media Data End...
      return (len);                                                     // Done

    if (msg_type_enc_get (msg_type) == NULL) {                          // If encrypted data...
      return (len);                                                     // Done
    }

    if (lft == 0)                                                       // If no content
      return (rmv);                                                     // Done

    if (lft < 2) {                                                      // If less than 2 bytes for content (impossible if content; need at least 1 byte for 0x08 and 1 byte for varint)
      loge ("hu_aad_dmp len: %d  lft: %d", len, lft);
      return (rmv);                                                     // Done with error
    }

    adj = iaad_dmp_n (1, 1, buf, lft);                                  // Dump the content w/ n=1

    iaad_adj (& rmv, & buf, & lft, adj);                                // Adjust past the content (to nothing, presumably)

    if (lft != 0 || rmv != len || rmv < 0)                              // If content left... (must be malformed)
      loge ("hu_aad_dmp after content len: %d  lft: %d  rmv: %d  buf: %p", len, lft, rmv, buf);

    logd ("--------------------------------------------------------");  // Empty line / 56 characters

    return (rmv);
  }
#endif

