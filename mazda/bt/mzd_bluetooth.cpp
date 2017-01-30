#include "mzd_bluetooth.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <array>
#include <iomanip>
#include <sstream>
#include <fstream>

// We can retrieve MAC Address from Mazda CMU from two sys paths:
//    /sys/fsl_otp/HW_OCOTP_MAC1 for the begining
//    /sys/fsl_otp/HW_OCOTP_MAC0 for the second part
// The beginning returns first two bytes of MAC, the second part the rest of MAC address as a HEX string.
// Example:
// echo /sys/fsl_otp/HW_OCOTP_MAC1 -> 0x88c6
// echo /sys/fsl_otp/HW_OCOTP_MAC0 -> 0x261b82f2
// Actual MAC: 88:c6:26:1b:82:f2

// Cache the address if we need to retrieve it repeatedly;
static std::string macAddress = "";

static uint32_t hexStrToInt(std::string hexStr) {
    uint32_t x;
    std::stringstream ss;
    ss << std::hex << hexStr;
    ss >> x;
    return x;
}

/**
*   Converts passed number into HEX pair for MAC address, e.g. 10 -> 0A
*/
static std::string formatNumber(uint32_t num) {
    std::stringstream str;
    if (num < 16) str << "0";   // MACs are padded with zeros... std::setw doesn't woork on HU.
    str << std::uppercase << std::hex;
    str << num;
    std::string out = str.str();
    return out;
}

std::string get_bluetooth_mac_address() {
    if (macAddress != "") return macAddress;

    std::ifstream macUpperFile("/sys/fsl_otp/HW_OCOTP_MAC1");
    std::ifstream macLowerFile("/sys/fsl_otp/HW_OCOTP_MAC0");

    std::stringstream macUpper;
    std::stringstream macLower;

    macUpper << macUpperFile.rdbuf();
    macLower << macLowerFile.rdbuf();

    // Check for I/O error
    if (macUpperFile.fail() || macLowerFile.fail()) return "";

    uint32_t macAddrUpper = hexStrToInt(macUpper.str());
    uint32_t macAddrLower = hexStrToInt(macLower.str());

    std::stringstream ss;
    ss << formatNumber((macAddrUpper >> 8) & 0xFF);
    ss << ":";
    ss << formatNumber(macAddrUpper & 0xFF);
    ss << ":";
    ss << formatNumber((macAddrLower >> 24) & 0xFF);
    ss << ":";
    ss << formatNumber((macAddrLower >> 16) & 0xFF);
    ss << ":";
    ss << formatNumber((macAddrLower >> 8) & 0xFF);
    ss << ":";
    ss << formatNumber((macAddrLower & 0xFF) + 1);  // Is BT address really always +1 from the base one in file?

    macAddress = ss.str();
    printf("Bluetooth MAC: %s\n", macAddress.c_str());
    return macAddress;
}