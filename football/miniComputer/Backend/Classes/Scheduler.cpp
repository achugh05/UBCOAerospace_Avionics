//
// Created by cline on 1/28/26.
//

#include "Scheduler.h"

#include <iostream>

Scheduler::Scheduler() {
    std::cout<<"Press enter to exit"<<std::endl;
    loadCFG();
    initNetworkConnection();
    initSerialConnection();

    std::cin.sync();
    std::cin.get();
}

Scheduler::~Scheduler() {
    TCPconn->closeConn();
    UDPconn->closeConn();
    logger->close();
}

void Scheduler::loadCFG() {
    configLoader l{"./config.toml"};
    bool loaded = l.load(&cfg);
    initLogger();
    l.setLogger(logger);
    if (!loaded) {
        l.createDefaultCFGfile(&cfg);
    }
    l.validate(&cfg);
}

void Scheduler::initLogger() {
    logger = std::make_shared<Logger>(cfg.loggingConfig.logging_path, cfg.basicConfig.devName, (VerbosityLevel) cfg.loggingConfig.logLevel);
}

void Scheduler::initNetworkConnection() {
    TCPconn = std::make_unique<NetworkLink>(Protocol::TCP,
        cfg.networkConfig.localAddr, cfg.networkConfig.local_port,
        cfg.networkConfig.remoteAddr,  cfg.networkConfig.remote_port,
        true);

    TCPconn->setCallback([this](CommPacket packet) {
        this->networkPacketCB(packet);
    });
    TCPconn->setCallback(onConnection, [this]() {
       this->nConnMadeCB();
    });
    TCPconn->setCallback(onDisconnection, [this]() {
        this->nConnLostCB();
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::STATION, "Initialized TCP server listening on port: " + std::to_string(cfg.networkConfig.local_port));

    UDPconn = std::make_unique<NetworkLink>(Protocol::UDP,
        cfg.networkConfig.localAddr, cfg.networkConfig.local_port,
        cfg.networkConfig.remoteAddr,  cfg.networkConfig.remote_port);
    UDPconn->setCallback([this](CommPacket packet) {
        this->networkPacketCB(packet);
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::STATION,"Initialized UDP connections listening on: "+std::to_string(cfg.networkConfig.local_port)+" sending to: "+cfg.networkConfig.remoteAddr+":"+std::to_string(cfg.networkConfig.remote_port));

    //FOR TESTING ONLYY
    // test = std::make_unique<NetworkLink>(Protocol::TCP,
    //    cfg.networkConfig.remoteAddr, cfg.networkConfig.remote_port,
    //    cfg.networkConfig.localAddr,  cfg.networkConfig.local_port,
    //    false);
    //
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    // messageData t = CommPacket(lora_station,miniPC,ignition).toMessage();
    // test->sendData(t.data(), t.size());
}

void Scheduler::initSerialConnection() {
    Serialconn = std::make_unique<Serial>(cfg.serialConfig.port.c_str(), cfg.serialConfig.speed, logger);

    Serialconn->setCallback([this](CommPacket packet) {
        this->serialPacketCB(packet);
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::STATION, "Initialized Serial connections listening on port: "+cfg.serialConfig.port);
}

void Scheduler::networkPacketCB(CommPacket packet) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Message Received by network: " + packet.toString());
}

void Scheduler::nConnMadeCB() {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Successfully Connected (TCP)");
}

void Scheduler::nConnLostCB() {
    logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Unexpected disconnection occured, attempting recconnection (TCP)");
}

void Scheduler::processCommandPacket(CommPacket packet) {

}

void Scheduler::processTelemetryPacket(CommPacket packet) {
}



void Scheduler::serialPacketCB(CommPacket packet) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Test Json Received from serial: ");
}

