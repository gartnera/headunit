//
// Created by silverchris on 2020-11-06.
//

#include "wireless.h"

#include <cstdint>
#include <string>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <future>

#include "hu.pb.h"
#include "../main.h"

#include "hu_uti.h"

BDSClient *bds_client = nullptr;

BCAClient *bca_client = nullptr;

char IP_ADDRESS[16];
char MAC_ADDRESS[18];

std::promise<int> * promise;


void sendMessage(int fd, google::protobuf::MessageLite &message, uint16_t type) {
    auto byteSize = static_cast<size_t>(message.ByteSizeLong());
    auto sizeOut = static_cast<uint16_t>(htobe16(byteSize));
    auto typeOut = static_cast<uint16_t>(htobe16(type));
    char *out = (char *) malloc(byteSize + 4);
    memcpy(out, &sizeOut, 2);
    memcpy(out + 2, &typeOut, 2);

    message.SerializeToArray(out + 4, byteSize);

    auto written = write(fd, out, byteSize + 4);
    if (written > -1) {
        logd("Bytes written: %u", written);
    } else {
        loge("Could not write data");
    }
    free(out);
}

void handleWifiInfoRequest(int fd, uint8_t *buffer, uint16_t length) {
    HU::WifiInfoRequest msg;
    msg.ParseFromArray(buffer, length);
    logd("WifiInfoRequest: %s", msg.DebugString().c_str());

    HU::WifiInfoResponse response;
    response.set_ip_address(IP_ADDRESS);
    response.set_port(30515);
    response.set_status(HU::WifiInfoResponse_Status_STATUS_SUCCESS);

    sendMessage(fd, response, 7);
}

void handleWifiSecurityRequest(int fd, __attribute__((unused)) uint8_t *buffer, __attribute__((unused)) uint16_t length) {
    HU::WifiSecurityReponse response;

    response.set_ssid(hostapd_config("ssid").c_str());
    response.set_bssid(MAC_ADDRESS);
    response.set_key(hostapd_config("wpa_passphrase").c_str());
    response.set_security_mode(HU::WifiSecurityReponse_SecurityMode_WPA2_PERSONAL);
    response.set_access_point_type(HU::WifiSecurityReponse_AccessPointType_DYNAMIC);

    sendMessage(fd, response, 3);
}

int handleWifiInfoRequestResponse(int  __attribute__((unused)) fd, uint8_t *buffer, uint16_t length) {
    HU::WifiInfoResponse msg;
    msg.ParseFromArray(buffer, length);
    logd("WifiInfoResponse: %s", msg.DebugString().c_str());
    return msg.status();
}

void handle_connect(char *pty){
    char buf[100];
    logd("\tPTY: %s", pty);
    int fd = open(pty, O_RDWR | O_NOCTTY | O_SYNC);
    HU::WifiInfoRequest request;
    request.set_ip_address(IP_ADDRESS);
    request.set_port(30515);

    sendMessage(fd, request, 1);

    promise->set_value(HU_MODE::WIFI_AP);

    ssize_t len = 0;
    int loop = 1;
    while (loop) {
        ssize_t i = read(fd, buf, 4);
        len += i;
        if (len >= 4) {
            auto size = static_cast<uint16_t>(be16toh(*(uint16_t *) buf));
            auto type = static_cast<uint16_t>(be16toh(*(uint16_t * )(buf + 2)));
            logd("Size: %u, MessageID: %u, left: %u", size, type);
            if (len >= size + 4) {
                auto *buffer = (uint8_t *) malloc(size);
                i = 0;
                while (i < size) {
                    i += read(fd, buffer, size);
                }
                switch (type) {
                    case 1:
                        handleWifiInfoRequest(fd, buffer, size);
                        break;
                    case 2:
                        handleWifiSecurityRequest(fd, buffer, size);
                        break;
                    case 7:
                        if (handleWifiInfoRequestResponse(fd, buffer, size) == 0) {
                            loop = 0;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

void BCAClient::ConnectionStatusResp(
        const uint32_t &serviceId,
        const uint32_t &connStatus,
        const uint32_t &btDeviceId,
        const uint32_t &status,
        const ::DBus::Struct <std::vector<uint8_t>> &terminalPath) {
    logd("Saw Service: %u", serviceId);
    if (serviceId == 15 && connStatus == 3) {
        update_ip_mac();
        char *pty = (char *) malloc(terminalPath._1.size());
        std::copy(terminalPath._1.begin(), terminalPath._1.end(), pty);
        logd("\tPTY: %s", pty);
        handle_connect(pty);
        free(pty);
    }
}

void BDSClient::SignalConnected_cb(const uint32_t &type, const ::DBus::Struct <std::vector<uint8_t>> &data) {
    char pty[100];
    logd("Saw Service: %u", data._1[36]);
    if (data._1[36] == 15) {
        update_ip_mac();
        std::strncpy(pty, (char *) &data._1[48], 100);
        logd("\tPTY: %s", pty);
        handle_connect(pty);
    }
}

std::string hostapd_config(const std::string& key) {
    std::ifstream hostapd_file;
    hostapd_file.open("/tmp/current-session-hostapd.conf");

    if (hostapd_file) {
        std::string line;
        size_t pos;
        while (hostapd_file.good()) {
            getline(hostapd_file, line); // get line from file
            if (line[0] != '#') {
                pos = line.find(key); // search
                if (pos != std::string::npos) // string::npos is returned if string is not found
                {
                    size_t equalPosition = line.find('=');
                    std::string value = line.substr(equalPosition + 1);
                    return value;
                }
            }
        }
        return "";
    }
    else {
        return "";
    }
}




void wireless_stop(){
    delete bds_client;
    delete bca_client;
}

void update_ip_mac(){
    int fd;
    struct ifreq ifr{};
    char iface[] = "wlan0";
    unsigned char *mac;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);

    ioctl(fd, SIOCGIFHWADDR, &ifr);
    mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    sprintf(MAC_ADDRESS, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x" , mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ioctl(fd, SIOCGIFADDR, &ifr);
    sprintf(IP_ADDRESS, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

    close(fd);

    logd("Got IP: %s MAC: %s", IP_ADDRESS, MAC_ADDRESS);
}


void wireless_thread(std::promise<int> * promObj, DBus::Connection *serviceBus, DBus::Connection *hmiBus) {
    promise = promObj;

    struct stat buffer{};
    while(stat("/tmp/current-session-hostapd.conf", &buffer) != 0){
        sleep(1);
    }
    while(stat("/tmp/current-session-udhcpd.conf", &buffer) != 0){
        sleep(1);
    }

    bds_client = new BDSClient(*serviceBus, "/com/jci/bds", "com.jci.bds");
    bca_client = new BCAClient(*hmiBus, "/com/jci/bca", "com.jci.bca");

    bca_client->StartAdd(15);


}
