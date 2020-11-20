
#include "hu_aap.h"
#include <netinet/in.h>

class HUTransportStreamTCP : public HUTransportStream
{
    int tcp_so_fd = -1;
    struct sockaddr_in  cli_addr = {0};
    socklen_t cli_len = 0;
    struct sockaddr_in  srv_addr = {0};
    socklen_t srv_len = 0;
    std::string& phoneIpAddress;

    int wifi_direct = 0;//0;
    int itcp_deinit ();
    int itcp_accept ();
    int itcp_init();
 public:
    ~HUTransportStreamTCP();
    HUTransportStreamTCP(std::string& IpAddress);
    virtual int Start(bool waitForDevice) override;
    virtual int Stop() override;
    virtual int Write(const byte* buf, int len, int tmo) override;
};
