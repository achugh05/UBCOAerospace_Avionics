//
// Created by cline on 1/28/26.
//

#include "../include/Scheduler.h"

#include <iostream>

Scheduler::Scheduler() {
    std::cout<<"Press enter to exit"<<std::endl;
    loadCFG();
    initConnectedSystems();
    initSerialConnection();
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
    TCPconn = std::make_unique<NetworkLink>(logger, Protocol::TCP,
        cfg.networkConfig.localAddr, cfg.networkConfig.local_port,
        cfg.networkConfig.remoteAddr,  cfg.networkConfig.remote_port,
        true);

    TCPconn->setCallback([this](CommPacket packet) {
        this->networkPacketTCP_CB(packet);
    });
    TCPconn->setCallback(onConnection, [this]() {
       this->nConnMadeCB();
    });
    TCPconn->setCallback(onDisconnection, [this]() {
        this->nConnLostCB();
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::FOOTBALL, "Initialized TCP server listening on port: " + std::to_string(cfg.networkConfig.local_port));

    UDPconn = std::make_unique<NetworkLink>(logger, Protocol::UDP,
        cfg.networkConfig.localAddr, cfg.networkConfig.local_port,
        cfg.networkConfig.remoteAddr,  cfg.networkConfig.remote_port);
    UDPconn->setCallback([this](CommPacket packet) {
        this->networkPacketUDP_CB(packet);
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::FOOTBALL,"Initialized UDP connections listening on: "+std::to_string(cfg.networkConfig.local_port)+" sending to: "+cfg.networkConfig.remoteAddr+":"+std::to_string(cfg.networkConfig.remote_port));

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
    Serialconn = std::make_unique<Serial>(cfg.serialConfig.port.c_str(), cfg.serialConfig.speed, logger);

    Serialconn->setCallback([this](CommPacket packet) {
        this->serialPacketCB(packet);
    });

    logger->println(VerbosityLevel::INFO, SubsystemTag::STATION, "Initialized Serial connections listening on port: "+cfg.serialConfig.port);

    // //FOR TESTING ONLY
    // Serial test = Serial("/dev/pts/4", cfg.serialConfig.speed, logger);
    // std::vector<uint16_t> d ={15,321};
    //
    // messageData t = TelemetryData(miniPC, miniPC, d, cfg.telemetry_config.numServos, cfg.telemetry_config.numPressure).toMessage();
    //
    // test.sendPacket(t.data(), t.size());
}

void Scheduler::initConnectedSystems() {
    for (auto s: cfg.managementConfig.systemsInNetwork) {
        connectedSubsystems[s] = Subsystem(s);
    }
}

void Scheduler::networkPacketTCP_CB(CommPacket packet) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Message Received by network (TCP): " + packet.toString());

    if (validatePacket(packet)) {
        switch (processPacket(packet)) {
            case passOn: {
                auto message = packet.toMessage();
                Serialconn->sendPacket(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Passing message on to: "+getSystemIdName(packet.SourceID));
            } break;
            case resolved : {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Processed message successfully");
            } break;
            case sendAcknowledge: {
                auto message = CommPacket::makeAcknowledgementPacket(miniPC, (SystemIDs) packet.SourceID).toMessage();
                TCPconn->sendData(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Sending acknowledgement packet to: "+getSystemIdName(packet.SourceID));
            } break;
            case failed: {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Failed to process packet: "+packet.toString());
            } break;
        }
    } else {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Packet received from source is malformed");
    }
}
void Scheduler::networkPacketUDP_CB(CommPacket packet) {
}

void Scheduler::nConnMadeCB() {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Successfully Connected (TCP)");
}

void Scheduler::nConnLostCB() {
    logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Unexpected disconnection occured, attempting recconnection (TCP)");
}

bool Scheduler::validatePacket(CommPacket p) {
    return p.validate();
}

commandResponseAction Scheduler::processPacket(CommPacket p) {
    // if (p.Destination_ID != miniPC) {
    //     return passOn;
    // }

    connectedSubsystems[getSystemIdName(p.SourceID)].lastReceivedMessage = p;

    commandResponseAction completed;
    switch (p.getPacketType()) {
        case CommandPacket: {
            completed = processCommandPacket(p);
        } break;
        case TelemetryPacket: {
            completed = processTelemetryPacket(p);
        } break;
        case AcknowledgementPacket: {
            completed = processAcknoledgementPacket(p);
        } break;
        case ErrorPacket: {
            completed = processErrorPacket(p);
        } break;
    }

    return completed;
}


commandResponseAction Scheduler::processCommandPacket(CommPacket packet) {
    //TODO:: IDK what commands the mini PC needs to handel rn

    logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Received Executable command "+ packet.toString());

    return sendAcknowledge;
}

commandResponseAction Scheduler::processTelemetryPacket(CommPacket packet) {
    TelemetryData td = TelemetryData(packet, cfg.telemetry_config.numServos, cfg.telemetry_config.numPressure);
    logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Processed Telemetry Packet "+ td.toString());


    return resolved;
}

commandResponseAction Scheduler::processErrorPacket(CommPacket p) {
    auto ErroredSystem = &connectedSubsystems[getSystemIdName(p.SourceID)];
    ErroredSystem->errorCode = p.MessageID;
    ErroredSystem->state = critical;

    //TODO:: do other warning / tracking stuff that I havent figured out yet.

    logger->println(VerbosityLevel::FATAL, sourceIDtoSubsystemTag(p.SourceID), p.toErrorString());

    return resolved;
}

commandResponseAction Scheduler::processAcknoledgementPacket(CommPacket p) {
    //TODO:: Handle Later when figured out stuff

    return resolved;
}


void Scheduler::serialPacketCB(CommPacket packet) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::COMMS, "Test Json Received from serial: "+packet.toString());

    if (validatePacket(packet)) {
        switch (processPacket(packet)) {
            case passOn: {
                auto message = packet.toMessage();
                TCPconn->sendData(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Passing message on to: "+getSystemIdName(packet.SourceID));
            } break;
            case resolved : {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Processed message successfully");
            } break;
            case sendAcknowledge: {
                auto message = CommPacket::makeAcknowledgementPacket(miniPC, (SystemIDs) packet.SourceID).toMessage();
                Serialconn->sendPacket(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Sending acknowledgement packet to: "+getSystemIdName(packet.SourceID));
            } break;
            case failed: {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Failed to process packet: "+packet.toString());
            } break;
        }
    } else {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Packet received from source is malformed");
    }
}

