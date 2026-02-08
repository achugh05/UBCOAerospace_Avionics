//
// Created by cline on 1/28/26.
//

#include "Scheduler.h"

#include <iostream>

Scheduler::Scheduler() {
    std::cout<<"Press enter to exit"<<std::endl;
    loadCFG();
    initNetworkConnection();

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
        "127.0.0.1", cfg.networkConfig.local_port,
        cfg.networkConfig.remoteAddr,  cfg.networkConfig.remote_port,
        true);

    TCPconn->setCallback(onJsonReceive, [this](std::string buf) {
        this->networkPacketCB(buf);
    });
    TCPconn->setCallback(onConnection, [this](std::string _) {
       this->nConnMadeCB();
    });
    TCPconn->setCallback(onDisconnection, [this](std::string _) {
        this->nConnLostCB();
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::STATION, "Initialized TCP server listening on port: " + std::to_string(cfg.networkConfig.local_port));

    UDPconn = std::make_unique<NetworkLink>(Protocol::UDP,
        "127.0.0.1", cfg.networkConfig.local_port,
        cfg.networkConfig.remoteAddr,  cfg.networkConfig.remote_port);
    UDPconn->setCallback(onJsonReceive, [this](std::string buf) {
        this->networkPacketCB(buf);
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::STATION,"Initialized UDP connections listening on: "+std::to_string(cfg.networkConfig.local_port)+" sending to: "+cfg.networkConfig.remoteAddr+":"+std::to_string(cfg.networkConfig.remote_port));
}

void Scheduler::networkPacketCB(std::string buf) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Test Json Received by network: "+buf);
}

void Scheduler::nConnMadeCB() {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Successfully Connected (TCP)");
}

void Scheduler::nConnLostCB() {
    logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Unexpected disconnection occured, attempting recconnection (TCP)");
}


void Scheduler::serialPacketCB(std::string buf) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Test Json Received from serial: "+buf);
}

