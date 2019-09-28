#include <fstream>
#include "hu_aap.h"
#include "config.h"

//default settings
std::string config::configFile = "/tmp/root/headunit.json";
bool config::launchOnDevice = true;
bool config::carGPS = true;
HU_TRANSPORT_TYPE config::transport_type = HU_TRANSPORT_TYPE::USB;
std::string config::phoneIpAddress = "192.168.43.1";
bool config::reverseGPS = false;

void config::parseJson(json config_json)
{
    if (config_json["launchOnDevice"].is_boolean())
    {
        config::launchOnDevice = config_json["launchOnDevice"];
    }
    if (config_json["carGPS"].is_boolean())
    {
        config::carGPS = config_json["carGPS"];
    }
    if (config_json["wifiTransport"].is_boolean())
    {
        config::transport_type = config_json["wifiTransport"] ? HU_TRANSPORT_TYPE::WIFI : HU_TRANSPORT_TYPE::USB;
    }
    if (config_json["phoneIpAddress"].is_string())
    {
        config::phoneIpAddress = config_json["phoneIpAddress"];
    }
    if (config_json["reverseGPS"].is_boolean())
    {
        config::reverseGPS = config_json["reverseGPS"];
    }
    printf("json config parsed\n");
}

json config::readConfigFile()
{
    std::ifstream ifs(config::configFile);
    json config_json;
    //config file exists, read it
    if (ifs.good())
    {
        try
        {
            ifs >> config_json;
        }catch (...)
        {
            printf("couldn't parse config file. possible corruption\n");
            config_json = nullptr;
            ifs.close();
        }
        ifs.close();
    }
    else
    {
        printf("couldn't read config file. check permissions\n");
    }
    return config_json;
}

void config::readConfig()
{
    json config_json = readConfigFile();
    if (config_json == nullptr) return;

    config::parseJson(config_json);
}

void config::writeConfigFile(json config_json)
{
    std::ofstream out_file(config::configFile);
    if (out_file.good())
    {
        out_file << std::setw(4) << config_json << std::endl;
        out_file.close();
        printf("config file written\n");
    }
    else
    {
        printf("couldn't write config file. check permissions\n");
    }
}

void config::updateConfigString(std::string parameter, std::string value)
{
    printf("updating parameter [%s] = [%s]\n", parameter.c_str(), value.c_str());
    json config_json = readConfigFile();
    if (config_json == nullptr) return;

    config_json[parameter]=value;
    writeConfigFile(config_json);
    config::parseJson(config_json);
}

void config::updateConfigBool(std::string parameter, bool value)
{
    printf("updating parameter [%s] = [%s]\n", parameter.c_str(), value ? "true" : "false");
    json config_json = readConfigFile();
    if (config_json == nullptr) return;

    config_json[parameter]=value;
    writeConfigFile(config_json);
    config::parseJson(config_json);
}
