#pragma once

#include "web++/web++.hpp"

//These happen in the web server thread
class ICommandServerCallbacks
{
public:
    virtual bool IsConnected() const = 0;
    virtual bool HasAudioFocus() const = 0;
    virtual bool HasVideoFocus() const = 0;
    virtual void TakeVideoFocus() = 0;
    virtual std::string GetLogPath() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual std::string ChangeParameterConfig(std::string param, std::string value, std::string type) const = 0;
};

//This is mostly designed as a way to recieve UI events from the CMU JS code via HTTP requests
class CommandServer
{
    WPP::Server server;

public:
    CommandServer(ICommandServerCallbacks& callbacks);

    bool Start();
    void Stop();
};
