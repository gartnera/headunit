#pragma once

#include "json/json.hpp"
using json = nlohmann::json;

class config {
public:

    static void readConfig();
    static void updateConfigString(std::string parameter, std::string value);
    static void updateConfigBool(std::string parameter, bool value);

    static std::string configFile;
    static bool launchOnDevice;
    static bool carGPS;
    static HU_TRANSPORT_TYPE transport_type;
    static std::string phoneIpAddress;
    static bool reverseGPS;

private:
    static json readConfigFile();
    static void writeConfigFile(json config_json);
    static void parseJson(json config_json);
};

