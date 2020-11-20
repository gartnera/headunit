#include "command_server.h"
#include "json/json.hpp"

#include "hu_uti.h"

using json = nlohmann::json;

namespace
{
	void AddCORSHeaders(WPP::Response& resp)
	{
	    resp.headers.emplace("Access-Control-Allow-Origin", "*");
	    resp.headers.emplace("Access-Control-Allow-Methods", "POST, GET");
	}
}

CommandServer::CommandServer(ICommandServerCallbacks &callbacks)
{
    server.get("/status", [&callbacks](WPP::Request& req, WPP::Response& resp)
    {
        resp.type = "application/json";
        json result;
        result["connected"]  = callbacks.IsConnected();
        result["videoFocus"] = callbacks.HasVideoFocus();
        result["audioFocus"] = callbacks.HasAudioFocus();
        std::string logPath = callbacks.GetLogPath();
        if (logPath.length() > 0)
        {
            result["logPath"] = logPath;
        }
        result["headunitVersion"] = callbacks.GetVersion();

        resp.body << std::setw(4) << result;

        logd("Got /status call. response:\n%s\n", resp.body.str().c_str());

        AddCORSHeaders(resp);
    });

    server.get("/updateConfig", [&callbacks](WPP::Request& req, WPP::Response& resp)
    {
        resp.type = "application/json";
        json result;
        result["result"] = callbacks.ChangeParameterConfig(req.query["parameter"], req.query["value"], req.query["type"]);

        resp.body << std::setw(4) << result;

        logd("Got /updateConfig call. response:\n%s\n", resp.body.str().c_str());

        AddCORSHeaders(resp);
    });

    server.post("/takeVideoFocus", [&callbacks](WPP::Request& req, WPP::Response& resp)
    {
        resp.type = "application/json";
        callbacks.TakeVideoFocus();
        json result;
        resp.body << std::setw(4) << result;

        printf("Got /takeVideoFocus call. response:\n%s\n", resp.body.str().c_str());

        AddCORSHeaders(resp);
    });
}

bool CommandServer::Start()
{
    return server.start(9999);
}

void CommandServer::Stop(){
    server.stop();
}