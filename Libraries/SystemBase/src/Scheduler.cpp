//
// Created by cline on 1/28/26.
//

#include "../include/Scheduler.h"

#include <iostream>


void Scheduler::init(bool isServer) {
    loadCFG();
    initSerialConnection();
    initNetworkConnection(isServer);
}

void Scheduler::loadCFG() {
    configLoader l{"./config.toml"};
    bool loaded = l.load(cfgRef);
    initLogger();
    l.setLogger(logger);
    if (!loaded) {
        l.createDefaultCFGfile(cfgRef);
    }
    l.validate(cfgRef);
}

void Scheduler::initLogger() {
    logger = std::make_shared<Logger>(cfgRef->loggingConfig.logging_path, cfgRef->basicConfig.devName, (VerbosityLevel) cfgRef->loggingConfig.logLevel);
}

void Scheduler::initNetworkConnection(bool isServer) {
    TCPconn = std::make_unique<NetworkLink>(logger, Protocol::TCP,
        cfgRef->networkConfig.localAddr, cfgRef->networkConfig.local_port,
        cfgRef->networkConfig.remoteAddr,  cfgRef->networkConfig.remote_port,
        isServer);

    TCPconn->setCallback([this](CommPacket packet) {
        this->networkPacketTCP_CB(packet);
    });
    TCPconn->setCallback(onConnection, [this]() {
       this->nConnMadeCB();
    });
    TCPconn->setCallback(onDisconnection, [this]() {
        this->nConnLostCB();
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::FOOTBALL, "Initialized TCP server listening on port: " + std::to_string(cfgRef->networkConfig.local_port));

    UDPconn = std::make_unique<NetworkLink>(logger, Protocol::UDP,
        cfgRef->networkConfig.localAddr, cfgRef->networkConfig.local_port,
        cfgRef->networkConfig.remoteAddr,  cfgRef->networkConfig.remote_port);
    UDPconn->setCallback([this](CommPacket packet) {
        this->networkPacketUDP_CB(packet);
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::FOOTBALL,"Initialized UDP connections listening on: "+std::to_string(cfgRef->networkConfig.local_port)+" sending to: "+cfgRef->networkConfig.remoteAddr+":"+std::to_string(cfgRef->networkConfig.remote_port));

     // //FOR TESTING ONLY
     // test = std::make_unique<NetworkLink>(logger, Protocol::TCP,
     //    cfg.networkConfig.remoteAddr, cfg.networkConfig.remote_port,
     //    cfg.networkConfig.localAddr,  cfg.networkConfig.local_port,
     //    false);
     //
     // std::this_thread::sleep_for(std::chrono::seconds(5));
     // std::vector<uint16_t> d ={15,321};
     //
     // messageData t = TelemetryData(miniPC, miniPC, d, cfg.telemetry_config.numServos, cfg.telemetry_config.numPressure).toMessage();
     //
     // test->sendData(t.data(), t.size());
}

void Scheduler::initSerialConnection() {
    Serialconn = std::make_unique<Serial>(cfgRef->serialConfig.port.c_str(), cfgRef->serialConfig.speed, logger);

    Serialconn->setCallback([this](CommPacket packet) {
        this->serialPacketCB(packet);
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::STATION, "Initialized Serial connections listening on port: "+cfgRef->serialConfig.port);

    // //FOR TESTING ONLY
    // Serial test = Serial("/dev/pts/4", cfg.serialConfig.speed, logger);
    // std::vector<uint16_t> d ={15,321};
    //
    // messageData t = TelemetryData(miniPC, miniPC, d, cfg.telemetry_config.numServos, cfg.telemetry_config.numPressure).toMessage();
    //
    // test.sendPacket(t.data(), t.size());
}