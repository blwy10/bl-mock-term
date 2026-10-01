// mtsrv_config.cpp                                                   -*-C++-*-
#include "mtsrv_config.h"

#include <mtcore_stringutil.h>

#include <string.h>

namespace mtsrv {

using mtcore::StringUtil;

ServerConfig::ServerConfig()
: listenAddress("127.0.0.1")
, port(8195)
, seed(0)
, tickIntervalMs(25)
, tickProbability(0.2)
, dropProbability(0)
, workerThreads(4)
, heartbeatTimeoutMs(30000)
, newsMinIntervalMs(2500)
, newsMaxIntervalMs(6000)
, newsBackfill(20)
, statsIntervalMs(60000)
, logLevel(mtcore::LogLevel::e_INFO)
{
}

void ServerConfig::printUsage(FILE *stream)
{
    fprintf(stream,
            "usage: mtserver [options]\n"
            "  --listen ADDR        interface to bind (127.0.0.1)\n"
            "  --port N             TCP port, 0 = ephemeral (8195)\n"
            "  --seed N             simulation seed, 0 = random (0)\n"
            "  --tick-ms N          ticker plant interval (25)\n"
            "  --tick-prob P        per-instrument tick probability (0.2)\n"
            "  --drop-prob P        simulated feed packet loss (0)\n"
            "  --workers N          request worker threads (4)\n"
            "  --heartbeat-ms N     idle client timeout (30000)\n"
            "  --news-min-ms N      min headline interval (2500)\n"
            "  --news-max-ms N      max headline interval (6000)\n"
            "  --news-backfill N    headlines generated at start (20)\n"
            "  --stats-ms N         statistics log interval (60000)\n"
            "  --log-file PATH      also log to PATH\n"
            "  --log-level LEVEL    TRACE|DEBUG|INFO|WARN|ERROR (INFO)\n");
}

int ServerConfig::parse(std::string *error, int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            return 1;
        }
        if (i + 1 >= argc) {
            *error = StringUtil::format("missing value for %s", arg);
            return -1;
        }
        const char *value = argv[++i];
        mtcore::Int64 n = 0;
        double d = 0;
        bool isInt = StringUtil::toInt64(&n, value) == 0;
        bool isDouble = StringUtil::toDouble(&d, value) == 0;

        if (!strcmp(arg, "--listen")) {
            listenAddress = value;
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
        else if (!strcmp(arg, "--tick-prob") && isDouble) {
            tickProbability = d;
        }
        else if (!strcmp(arg, "--drop-prob") && isDouble) {
            dropProbability = d;
        }
        else if (!isInt) {
            *error = StringUtil::format("bad option or value: %s %s",
                                        arg,
                                        value);
            return -1;
        }
        else if (!strcmp(arg, "--port"))          port = (int)n;
        else if (!strcmp(arg, "--seed"))          seed = (mtcore::Uint64)n;
        else if (!strcmp(arg, "--tick-ms"))       tickIntervalMs = (int)n;
        else if (!strcmp(arg, "--workers"))       workerThreads = (int)n;
        else if (!strcmp(arg, "--heartbeat-ms"))  heartbeatTimeoutMs = (int)n;
        else if (!strcmp(arg, "--news-min-ms"))   newsMinIntervalMs = (int)n;
        else if (!strcmp(arg, "--news-max-ms"))   newsMaxIntervalMs = (int)n;
        else if (!strcmp(arg, "--news-backfill")) newsBackfill = (int)n;
        else if (!strcmp(arg, "--stats-ms"))      statsIntervalMs = (int)n;
        else {
            *error = StringUtil::format("unknown option %s", arg);
            return -1;
        }
    }
    if (port < 0 || port > 65535 || tickIntervalMs <= 0
     || workerThreads <= 0) {
        *error = "option out of range";
        return -1;
    }
    return 0;
}

}  // close namespace mtsrv
