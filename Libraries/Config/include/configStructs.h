//
// Created by cline on 2/6/26.
//

#ifndef GSE_CONFIGS_H
#define GSE_CONFIGS_H

#include "configLoader.h"
#include "constants.h"
#include "toml++/toml.hpp"

struct defaults;

struct configBase {
    virtual ~configBase() {};
    virtual bool validate(defaults* d) {
        return true;
    }
    virtual void from_toml(const toml::table& t){}
    virtual toml::table to_toml() {
        return toml::table{};
    }
};

struct BasicConfig : configBase {
    std::string devName = "Unidentified Device";

    void from_toml(const toml::table& t) override{
        devName = t["devName"].value_or(devName);
    }

    toml::table to_toml() override{
        return toml::table{
            {"devName", devName}
        };
    }
};

struct NetworkConfig: configBase {
    std::string host = "localhost";
    int port = 9000;
    int timeout_ms = 500;

    bool validate(defaults* d) override;

    void from_toml(const toml::table& t) override{
        host = t["host"].value_or(host);
        port = t["port"].value_or(port);
        timeout_ms = t["timeout_ms"].value_or(timeout_ms);
    }

    toml::table to_toml() override{
        return toml::table{
            {"host", host},
            {"port", port},
            {"timeout_ms", port}
        };
    }
};

struct SerialConfig : configBase{

};

struct LoggingConfig :configBase{
    int logLevel = 0;
    std::string logging_path = "./";

    void from_toml(const toml::table& t) override{
        logLevel = t["logLevel"].value_or(logLevel);
        logging_path = t["logging_path"].value_or(logging_path);
    }

    toml::table to_toml() override{
        return toml::table{
            {"logLevel", logLevel},
            {"logging_path", logging_path}
        };
    }
};

struct defaults{
    NetworkConfig networkConfig;

    void from_toml(const toml::table& t){
        if (auto* nt = t["Network"].as_table()) {
            networkConfig.from_toml(*nt);
        }
    }

    toml::table to_toml(){
        return toml::table{
                    {"Network", networkConfig.to_toml()}
        };
    }
};

//VALIDATION FUNCTIONS
//___________________________________________________________________________________________________________________-
inline bool NetworkConfig::validate(defaults *d) {

    bool r = true;

    if (port <= MINPORT || port > MAXPORT) {
        if (configLoader::getLogger())
            configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Specified port outside safe operating range, Fallback to default value");
        port = d->networkConfig.port;
        r = false;
    }

    if (timeout_ms <= MINTIMEOUT) {
        if (configLoader::getLogger())
            configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Timeout value is lower than minimum safe value, Fallback to default value");
        timeout_ms = d->networkConfig.timeout_ms;
        r = false;
    }

    return r;
}
//END


struct systemConfig {
    defaults defaultValues;

    virtual ~systemConfig(){}

    virtual bool validate() {
        return true;
    }

    virtual bool from_toml(const toml::table& t) {
        return true;
    }
    virtual toml::table to_toml() {
        return toml::table{};
    }

};

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

        if (auto* nt = t["Defaults"].as_table()) {
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
            {"Defaults", defaultValues.to_toml()}
        };
    }
};

struct GroundStationConfig {

};


#endif //GSE_CONFIGS_H