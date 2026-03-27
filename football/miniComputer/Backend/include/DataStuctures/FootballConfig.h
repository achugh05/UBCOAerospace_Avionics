//
// Created by cline on 3/12/26.
//

#ifndef GSE_FOOTBALLCONFIG_H
#define GSE_FOOTBALLCONFIG_H
#include "CommunicationFrameworks.h"
#include "configStructs.h"


struct ManagementConfig:configBase {
    std::vector<std::string> systemsInNetwork = {"lora_station", "manifold1", "manifold2", "lora_football", "mega_football", "pi_station"}; //defaulted to version 1 systemIDs (not including capstone)

    bool validate(defaults *d) override {
        bool r = true;
        for (int i = 0; i < systemsInNetwork.size(); i++) {
            auto s = systemsInNetwork[i];
            if (!isValidSystemName(s)) {
                if (configLoader::getLogger())
                    configLoader::getLogger()->println(VerbosityLevel::WARNING, SubsystemTag::CONFIG, "Specified subsystem name "+ s +" is Invalid, removing from list");

                systemsInNetwork.erase(systemsInNetwork.begin() + i);
                r=false;
            }
        }
        return r;
    }

    void from_toml(const toml::table &t) override {
        if (auto* arr = t["systemsInNetwork"].as_array()) {
            systemsInNetwork.reserve(arr->size());
            for (auto&& elem : *arr) {
                if (auto v = elem.value<std::string>())
                    systemsInNetwork.push_back(*v);
            }
        }
    }

    toml::table to_toml() override{
        toml::table tbl;
        toml::array arr;
        for (auto s : systemsInNetwork)
            arr.push_back(s);

        tbl.insert("systemsInNetwork", arr);

        return tbl;
    }
};

struct TelemetryConfig: configBase {
    int numServos = 0;
    int numPressure = 0;

    bool validate(defaults *d) override {
        bool r = true;
        if (numServos < 0) {
            if (configLoader::getLogger())
                configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Number of servos is invalid (must be a positive number), defaulting to 0");

            numServos = 0;
            r = false;
        }

        if (numPressure < 0) {
            if (configLoader::getLogger())
                configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Number of pressure sensors is invalid (must be a positive number), defaulting to 0");

            numServos = 0;
            r = false;
        }

        return r;
    }

    void from_toml(const toml::table &t) override {
        numServos = t["numServos"].value_or(numServos);
        numPressure = t["numPressure"].value_or(numPressure);
    }

    toml::table to_toml() override {
        return toml::table{
            {"numServos", numServos},
            {"numPressure", numPressure}
        };
    }
};

struct FootballStationConfig:systemConfig {

    BasicConfig basicConfig;
    ManagementConfig managementConfig;
    CommunicationConfig commConfig;
    TelemetryConfig telemetry_config;
    NetworkConfig networkConfig;
    SerialConfig serialConfig;
    LoggingConfig loggingConfig;

    bool validate() override{
        return basicConfig.validate(&defaultValues)
        &&managementConfig.validate(&defaultValues)
        && commConfig.validate(&defaultValues)
        && telemetry_config.validate(&defaultValues)
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
        if (auto* nt = t["SystemManagement"].as_table()) {
            managementConfig.from_toml(*nt);
        } else {
            return false;
        }

        if (auto* nt = t["CommunicationStandards"].as_table()) {
            commConfig.from_toml(*nt);
        } else {
            return false;
        }
        if (auto* nt = t["Telemetry"].as_table()) {
            telemetry_config.from_toml(*nt);
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
                    {"SystemManagement", managementConfig.to_toml()},
                    {"CommunicationStandards", commConfig.to_toml()},
                    {"Telemetry", telemetry_config.to_toml()},
                    {"Network", networkConfig.to_toml()},
                    {"Logging", loggingConfig.to_toml()},
                    {"Serial", serialConfig.to_toml()},
                    {"_Defaults", defaultValues.to_toml()}
        };
    }
};


#endif //GSE_FOOTBALLCONFIG_H