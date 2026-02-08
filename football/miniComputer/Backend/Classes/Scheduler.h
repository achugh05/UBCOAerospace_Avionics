//
// Created by cline on 1/28/26.
//

#ifndef GSE_SCHEDULER_H
#define GSE_SCHEDULER_H

#include "configStructs.h"
#include "NetworkLink.h"

struct FootballStationConfig:systemConfig {
    BasicConfig basicConfig;
    NetworkConfig networkConfig;
    LoggingConfig loggingConfig;

    bool validate() override{
        return basicConfig.validate(&defaultValues)
        && networkConfig.validate(&defaultValues)
        && loggingConfig.validate(&defaultValues);
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
                {"_Defaults", defaultValues.to_toml()}
        };
    }
};

class Scheduler {
    FootballStationConfig cfg;
    std::shared_ptr<Logger> logger;

    //networkstuff

    std::unique_ptr<NetworkLink> TCPconn;
    std::unique_ptr<NetworkLink> UDPconn;

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
    void networkPacketCB(std::string buf);
    void serialPacketCB(std::string buf);

    void nConnMadeCB();
    void nConnLostCB();

};


#endif //GSE_SCHEDULER_H
