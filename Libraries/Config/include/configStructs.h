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

struct CommunicationConfig: configBase {
    int resendsBeforeFatal = 5;
    float timeWaitForAcknowledge = 2; //In Seconds

    bool validate(defaults *d) override;

    void from_toml(const toml::table& t) override {
        resendsBeforeFatal = t["num_times_to_resend_command"].value_or(resendsBeforeFatal);
        timeWaitForAcknowledge = t["seconds_to_wait_for_acknowledge"].value_or(timeWaitForAcknowledge);
    }

    toml::table to_toml() override {
        return toml::table{
            {"num_times_to_resend_command", resendsBeforeFatal},
            {"seconds_to_wait_for_acknowledge", timeWaitForAcknowledge}
        };
    }
};

struct NetworkConfig: configBase {
    std::string localAddr = "127.0.0.1";
    std::string remoteAddr = "127.0.0.1";
    int local_port = 9000;
    int remote_port = 9001;

    int timeout_ms = 500;

    bool validate(defaults* d) override;

    void from_toml(const toml::table& t) override{
        localAddr = t["localAddr"].value_or(localAddr);
        remoteAddr = t["remoteAddr"].value_or(remoteAddr);
        remote_port = t["remote_port"].value_or(remote_port);
        local_port = t["local_port"].value_or(local_port);
        timeout_ms = t["timeout_ms"].value_or(timeout_ms);
    }

    toml::table to_toml() override{
        return toml::table{
            {"localAddr", localAddr},
            {"remoteAddr", remoteAddr},
            {"local_port", local_port},
            {"remote_port", remote_port},
            {"timeout_ms", timeout_ms}
        };
    }
};

struct SerialConfig : configBase{
    std::string port = "/dev/ttyUSB0";
    int speed = 9600;

    bool validate(defaults* d) override;

    void from_toml(const toml::table& t) override{
        port = t["serial_port"].value_or(port);
        speed = t["serial_speed"].value_or(speed);
    }
    toml::table to_toml() override{
        return toml::table{
                {"serial_port", port},
                {"serial_speed", speed}
        };
    }
};

struct LoggingConfig :configBase{
    int logLevel = 1;
    std::string logging_path = "./logs";

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
    SerialConfig serialConfig;
    CommunicationConfig commConfig;

    void from_toml(const toml::table& t){
        if (auto* nt = t["Network"].as_table()) {
            networkConfig.from_toml(*nt);
        }
        if (auto* nt = t["Serial"].as_table()) {
            serialConfig.from_toml(*nt);
        }
        if (auto* nt = t["CommunicationStandards"].as_table()) {
            commConfig.from_toml(*nt);
        }
    }

    toml::table to_toml(){
        return toml::table{
                    {"Network", networkConfig.to_toml()},
            {"Serial", serialConfig.to_toml()},
            {"CommunicationStandards", commConfig.to_toml()}
        };
    }
};

//VALIDATION FUNCTIONS
//___________________________________________________________________________________________________________________-
inline bool NetworkConfig::validate(defaults *d) {

    bool r = true;

    if (remote_port <= MINPORT || remote_port > MAXPORT) {
        if (configLoader::getLogger())
            configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Specified port for remote outside safe operating range, Fallback to default value");
        remote_port = d->networkConfig.remote_port;
        r = false;
    }

    if (local_port <= MINPORT || local_port > MAXPORT) {
        if (configLoader::getLogger())
            configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Specified port for local outside safe operating range, Fallback to default value");
        local_port = d->networkConfig.local_port;
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

inline bool SerialConfig::validate(defaults *d) {
    bool r = true;

    if (strncmp(port.c_str(), "/dev/tty", 8) != 0) {
        if (configLoader::getLogger())
            configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Specified port for serial invalid format, Fallback to default value");
        port = d->serialConfig.port;
        r = false;
    }

    return r;
}

inline bool CommunicationConfig::validate(defaults *d) {
    bool r = true;
    if (resendsBeforeFatal <= 0) {
        if (configLoader::getLogger())
            configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Number of times to resend commands is invalid (must be a positive number), falling back to defaults");
        resendsBeforeFatal = d->commConfig.resendsBeforeFatal;
        r = false;
    }

    if (timeWaitForAcknowledge <= 0) {
        if (configLoader::getLogger())
            configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Number of seconds to wait for acknowledgement is invalid (must be a positive number), falling back to defaults");
        timeWaitForAcknowledge = d->commConfig.timeWaitForAcknowledge;
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

struct GroundStationConfig {

};


#endif //GSE_CONFIGS_H