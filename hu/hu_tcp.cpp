  #define LOGTAG "hu_tcp"
  #include "hu_uti.h"                                                  // Utilities
  #include "hu_tcp.h"

  int itcp_state = 0; // 0: Initial    1: Startin    2: Started    3: Stoppin    4: Stopped
  int last_errno = 0; //store last error printed

  int wifi_direct = 0;

  #include <sys/types.h>
  #include <sys/time.h>
  #include <unistd.h>
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <netinet/ip.h>
  #include <netinet/tcp.h>

  #include <netinet/in.h>
  #include <netdb.h>
  #include <arpa/inet.h>

  HUTransportStreamTCP::~HUTransportStreamTCP()
  {
    if (itcp_state != hu_STATE_STOPPED)
    {
      Stop();
    }
  }

  HUTransportStreamTCP::HUTransportStreamTCP(std::string& IpAddress): phoneIpAddress(IpAddress){
      if(strcmp(IpAddress.c_str(), "0.0.0.0") == 0){
          wifi_direct = 1;
      }
  }

  int HUTransportStreamTCP::Write (const byte * buf, int len, int tmo) {
    //int ret = itcp_bulk_transfer (itcp_ep_out, buf, len, tmo);      // milli-second timeout
    if (readfd < 0)
      return (-1);

    fd_set sock_set;
    FD_ZERO(&sock_set);
    FD_SET(readfd, &sock_set);

    timeval tv_timeout;
    tv_timeout.tv_sec = tmo / 1000;
    tv_timeout.tv_usec = tmo * 1000;

    int ret = select(readfd+1, NULL, &sock_set, NULL, &tv_timeout);
    if (ret <= 0)
      return ret;


    errno = 0;
    ret = write (readfd, buf, len);
    if (ret != len) {             // Write, if can't write full buffer...
      loge ("Error write  errno: %d (%s)", errno, strerror (errno));
      //ms_sleep (101);                                                 // Sleep 0.1 second to try to clear errors
    }
      //close (readfd);
    //}
    //close (tcp_so_fd);

    return (ret);
  }

  int HUTransportStreamTCP::itcp_deinit () {                                              // !!!! Need to better reset and wait a while to kill transfers in progress and auto-restart properly

    if (readfd >= 0)
      close (readfd);
    readfd = -1;

    if (tcp_so_fd >= 0)
      close (tcp_so_fd);
    tcp_so_fd = -1;

    logd ("Done");

    return (0);
  }


  #define CS_FAM   AF_INET

  #define CS_SOCK_TYPE    SOCK_STREAM
  #define   RES_DATA_MAX  65536

  int HUTransportStreamTCP::itcp_accept ()
  {

    if (tcp_so_fd < 0)
      return (-1);

    memset ((char *) & cli_addr, 0, sizeof (cli_addr));                 // ?? Don't need this ?
    //cli_addr.sun_family = CS_FAM;                                     // ""
    cli_len = sizeof (cli_addr);

    errno = 0;
    int ret = 0;
    if (wifi_direct) {
      readfd = accept (tcp_so_fd, (struct sockaddr *) & cli_addr, & cli_len);
      if (readfd < 0) {
        loge ("Error accept errno: %d (%s)", errno, strerror (errno));
        return (-1);
      }
    }
    else {
      cli_addr.sin_addr.s_addr = inet_addr(phoneIpAddress.c_str());
      cli_addr.sin_family = AF_INET;
      cli_addr.sin_port = htons (5277);
      //logd ("cli_len: %d  fam: %d  addr: 0x%x  port: %d",cli_len,cli_addr.sin_family, ntohl (cli_addr.sin_addr.s_addr), ntohs (cli_addr.sin_port));

      ret = connect (tcp_so_fd, (const struct sockaddr *) & cli_addr, cli_len);
      if (ret != 0)
      {
        if (errno != last_errno) //avoid spamming the log with the same error
        {
            loge ("Error connect errno: %d (%s)", errno, strerror (errno));
            last_errno = errno;
        }
        sleep(5);
        return (-1);
      }
      readfd = tcp_so_fd;
    }

    return (tcp_so_fd);
  }


  int HUTransportStreamTCP::itcp_init () {

    int net_port = 30515;

    int cmd_len = 0, ctr = 0;
    //struct hostent *hp;

    errno = 0;
    if ((tcp_so_fd = socket (CS_FAM, CS_SOCK_TYPE, 0)) < 0) {              // Create socket
      loge ("gen_server_loop socket  errno: %d (%s)", errno, strerror (errno));
      return (-1);
    }
    int flag = 1;
    int ret = setsockopt (tcp_so_fd, SOL_TCP, TCP_NODELAY, & flag, sizeof (flag));  // Only need this for IO socket from accept() ??
    if (ret != 0)
      loge ("setsockopt TCP_NODELAY errno: %d (%s)", errno, strerror (errno));
    else
      logd ("setsockopt TCP_NODELAY Success");

    if (wifi_direct) {
        memset ((char *) & srv_addr, 0, sizeof (srv_addr));

        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = htonl (INADDR_ANY);                      // Will bind to any/all Interfaces/IPs

        //errno = 0;
        //hp = gethostbyname ("localhost");
        //if (hp == 0) {
        //  loge ("Error gethostbyname  errno: %d (%s)", errno, strerror (errno));
        //  return (-2);
        //}
        //bcopy ((char *) hp->h_addr, (char *) & srv_addr.sin_addr, hp->h_length);
        srv_addr.sin_port = htons (net_port);
        srv_len = sizeof (struct sockaddr_in);

        logd ("srv_len: %d  fam: %d  addr: 0x%x  port: %d", srv_len, srv_addr.sin_family, ntohl (srv_addr.sin_addr.s_addr), ntohs (srv_addr.sin_port));
        errno = 0;
        if (bind (tcp_so_fd, (struct sockaddr *) & srv_addr, srv_len) < 0) {   // Bind socket to server address
          loge ("Error bind  errno: %d (%s)", errno, strerror (errno));
          loge ("Inet stream continuing despite bind error");               // OK to continue w/ Internet Stream
        }

        // Done after socket() and before bind() so don't repeat it here ?
        //if (tmo != 0)
        //  sock_tmo_set (tcp_so_fd, tmo);                                   // If polling mode, set socket timeout for polling every tmo milliseconds

      // Get command from client
        errno = 0;
        if (listen (tcp_so_fd, 5)) {                                           // Backlog= 5; likely don't need this
          loge ("Error listen  errno: %d (%s)", errno, strerror (errno));
          return (-4);
        }

        logd ("itcp_init Ready");
    }

    //readfd = -1;
    while (readfd < 0) {                                             // While we don't have an IO socket file descriptor...
      itcp_accept ();
    }
    logd ("itcp_accept done");

    return (0);
  }

  int HUTransportStreamTCP::Stop () {
    itcp_state = hu_STATE_STOPPIN;
    logd ("  SET: itcp_state: %d (%s)", itcp_state, state_get (itcp_state));
    int ret = itcp_deinit ();
    itcp_state = hu_STATE_STOPPED;
    logd ("  SET: itcp_state: %d (%s)", itcp_state, state_get (itcp_state));
    return (ret);
  }


  int HUTransportStreamTCP::Start(bool waitForDevice) {
    int ret = 0;

    if (itcp_state == hu_STATE_STARTED) {
      logd ("CHECK: itcp_state: %d (%s)", itcp_state, state_get (itcp_state));
      return (0);
    }

    //#include <sys/prctl.h>
    //ret = prctl (PR_GET_DUMPABLE, 1, 0, 0, 0);  // 1 = SUID_DUMP_USER
    //logd ("prctl ret: %d", ret);

    itcp_state = hu_STATE_STARTIN;
    logd ("  SET: itcp_state: %d (%s)", itcp_state, state_get (itcp_state));

    ret = itcp_init();
    if (ret < 0) {
      loge ("Error itcp_init");
      itcp_deinit ();
      itcp_state = hu_STATE_STOPPED;
      logd ("  SET: itcp_state: %d (%s)", itcp_state, state_get (itcp_state));
      return (-1);
    }
    logd ("OK itcp_init");

    itcp_state = hu_STATE_STARTED;
    logd ("  SET: itcp_state: %d (%s)", itcp_state, state_get (itcp_state));
    return (0);
  }
