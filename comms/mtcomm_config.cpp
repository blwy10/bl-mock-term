// mtcomm_config.cpp                                                  -*-C++-*-
#include "mtcomm_config.h"

#include <mtcore_stringutil.h>

#include <string.h>

namespace mtcomm {

using mtcore::StringUtil;

CommsConfig::CommsConfig()
: listenAddress("127.0.0.1")
, port(8194)
, upstreamHost("127.0.0.1")
, upstreamPort(8195)
, spawnServer(false)
, defaultConflateMs(0)
, maxConflateMs(5000)
, clientTimeoutMs(30000)
, heartbeatMs(5000)
, reconnectMinMs(250)
, reconnectMaxMs(5000)
, statsIntervalMs(60000)
, logLevel(mtcore::LogLevel::e_INFO)
{
}

void CommsConfig::printUsage(FILE *stream)
{
    fprintf(stream,
            "usage: mtcomm [options]\n"
            "  --listen ADDR         local interface (127.0.0.1)\n"
            "  --port N              local client port, 0 = ephemeral (8194)\n"
            "  --upstream-host HOST  mtserver host (127.0.0.1)\n"
            "  --upstream-port N     mtserver port (8195)\n"
            "  --spawn-server        launch mtserver.exe as a child process\n"
            "  --server-exe PATH     mtserver executable for --spawn-server\n"
            "  --conflate-ms N       default per-client conflation (0)\n"
            "  --client-timeout-ms N idle local client timeout (30000)\n"
            "  --heartbeat-ms N      upstream heartbeat interval (5000)\n"
            "  --reconnect-min-ms N  first reconnect delay (250)\n"
            "  --reconnect-max-ms N  maximum reconnect delay (5000)\n"
            "  --stats-ms N          statistics log interval (60000)\n"
            "  --log-file PATH       also log to PATH\n"
            "  --log-level LEVEL     TRACE|DEBUG|INFO|WARN|ERROR (INFO)\n");
}

int CommsConfig::parse(std::string *error, int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            return 1;
        }
        if (!strcmp(arg, "--spawn-server")) {
            spawnServer = true;
            continue;
        }
        if (i + 1 >= argc) {
            *error = StringUtil::format("missing value for %s", arg);
            return -1;
        }
        const char *value = argv[++i];
        mtcore::Int64 n = 0;
        bool isInt = StringUtil::toInt64(&n, value) == 0;

        if (!strcmp(arg, "--listen")) {
            listenAddress = value;
        }
        else if (!strcmp(arg, "--upstream-host")) {
            upstreamHost = value;
        }
        else if (!strcmp(arg, "--server-exe")) {
            serverExe = value;
        }
        else if (!strcmp(arg, "--log-file")) {
            logFile = value;
        }
        else if (!strcmp(arg, "--log-level")) {
            if (mtcore::LogLevel::fromString(value, &logLevel) != 0) {
                *error = StringUtil::format("bad log level '%s'", value);
                return -1;
            }
        }
        else if (!isInt) {
            *error = StringUtil::format("bad option or value: %s %s",
                                        arg,
                                        value);
            return -1;
        }
        else if (!strcmp(arg, "--port"))              port = (int)n;
        else if (!strcmp(arg, "--upstream-port"))     upstreamPort = (int)n;
        else if (!strcmp(arg, "--conflate-ms"))       defaultConflateMs = (int)n;
        else if (!strcmp(arg, "--client-timeout-ms")) clientTimeoutMs = (int)n;
        else if (!strcmp(arg, "--heartbeat-ms"))      heartbeatMs = (int)n;
        else if (!strcmp(arg, "--reconnect-min-ms"))  reconnectMinMs = (int)n;
        else if (!strcmp(arg, "--reconnect-max-ms"))  reconnectMaxMs = (int)n;
        else if (!strcmp(arg, "--stats-ms"))          statsIntervalMs = (int)n;
        else {
            *error = StringUtil::format("unknown option %s", arg);
            return -1;
        }
    }
    if (port < 0 || port > 65535 || upstreamPort <= 0 || upstreamPort > 65535
     || reconnectMinMs <= 0 || reconnectMaxMs < reconnectMinMs) {
        *error = "option out of range";
        return -1;
    }
    return 0;
}

}  // close namespace mtcomm
