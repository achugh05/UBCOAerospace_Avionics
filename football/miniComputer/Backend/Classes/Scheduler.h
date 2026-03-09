//
// Created by cline on 1/28/26.
//

#ifndef GSE_SCHEDULER_H
#define GSE_SCHEDULER_H

#include "configStructs.h"
#include "NetworkLink.h"
#include "Serial.h"

struct FootballStationConfig:systemConfig {
    BasicConfig basicConfig;
    NetworkConfig networkConfig;
    SerialConfig serialConfig;
    LoggingConfig loggingConfig;

    bool validate() override{
        return basicConfig.validate(&defaultValues)
        && networkConfig.validate(&defaultValues)
        && loggingConfig.validate(&defaultValues)
        && serialConfig.validate(&defaultValues);
    }

    bool from_toml(const toml::table& t) override{
        if (auto* nt = t["Basic"].as_table()) {
            basicConfig.from_toml(*nt);
        } else {
            return false;
        }
        if (auto* nt = t["Network"].as_table()) {
            networkConfig.from_toml(*nt);
        } else {
            return false;
        }
        if (auto* nt = t["Logging"].as_table()) {
            loggingConfig.from_toml(*nt);
        } else {
            return false;
        }

        if (auto* nt = t["Serial"].as_table()) {
            serialConfig.from_toml(*nt);
        } else
            return false;

        if (auto* nt = t["_Defaults"].as_table()) {
            defaultValues.from_toml(*nt);
        } else {
            return false;
        }

        return true;
    }

    toml::table to_toml() override{
        return toml::table{
                {"Basic", basicConfig.to_toml()},
                {"Network", networkConfig.to_toml()},
                {"Logging", loggingConfig.to_toml()},
                {"Serial", serialConfig.to_toml()},
                {"_Defaults", defaultValues.to_toml()}
        };
    }
};

class Scheduler {
    FootballStationConfig cfg;
    std::shared_ptr<Logger> logger;

    //networkstuff

    std::unique_ptr<NetworkLink> TCPconn;
    std::unique_ptr<NetworkLink> test;
    std::unique_ptr<NetworkLink> UDPconn;

    std::unique_ptr<Serial> Serialconn;

public:
    Scheduler();
    ~Scheduler();

private:
    void loadCFG();
    void initLogger();
    void initNetworkConnection();
    void initSerialConnection();

public:
    //Callbacks

    void networkPacketCB(CommPacket packet);
    void serialPacketCB(CommPacket packet);

    void nConnMadeCB();
    void nConnLostCB();
private:
    void processCommandPacket(CommPacket p);
    void processTelemetryPacket(CommPacket p);
};


#endif //GSE_SCHEDULER_H
