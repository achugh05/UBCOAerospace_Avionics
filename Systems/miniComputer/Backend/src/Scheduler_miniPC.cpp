//
// Created by cline on 1/28/26.
//

#include "../include/Scheduler_miniPC.h"

#include <iostream>

Scheduler_miniPC::Scheduler_miniPC() {
    std::cout<<"Press enter to exit"<<std::endl;
    cfgRef = &cfg;
    init(true);

    std::cin.sync();
    std::cin.get();
}

Scheduler_miniPC::~Scheduler_miniPC() {
    TCPconn->closeConn();
    UDPconn->closeConn();
    logger->close();


}

void Scheduler_miniPC::initConnectedSystems() {
    for (auto s: cfg.managementConfig.systemsInNetwork) {
        connectedSubsystems[s] = Subsystem(s);
    }
}

void Scheduler_miniPC::networkPacketTCP_CB(CommPacket packet) {
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
void Scheduler_miniPC::networkPacketUDP_CB(CommPacket packet) {
}

void Scheduler_miniPC::nConnMadeCB() {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Successfully Connected (TCP)");
}

void Scheduler_miniPC::nConnLostCB() {
    logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Unexpected disconnection occured, attempting recconnection (TCP)");
}

commandResponseAction Scheduler_miniPC::processPacket(CommPacket p) {
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


commandResponseAction Scheduler_miniPC::processCommandPacket(CommPacket packet) {
    //TODO:: IDK what commands the mini PC needs to handel rn

    logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Received Executable command "+ packet.toString());

    return sendAcknowledge;
}

commandResponseAction Scheduler_miniPC::processTelemetryPacket(CommPacket packet) {
    TelemetryData td = TelemetryData(packet, cfg.telemetry_config.numServos, cfg.telemetry_config.numPressure);
    logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Processed Telemetry Packet "+ td.toString());


    return resolved;
}

commandResponseAction Scheduler_miniPC::processErrorPacket(CommPacket p) {
    auto ErroredSystem = &connectedSubsystems[getSystemIdName(p.SourceID)];
    ErroredSystem->errorCode = p.MessageID;
    ErroredSystem->state = critical;

    //TODO:: do other warning / tracking stuff that I havent figured out yet.

    logger->println(VerbosityLevel::FATAL, sourceIDtoSubsystemTag(p.SourceID), p.toErrorString());

    return resolved;
}

commandResponseAction Scheduler_miniPC::processAcknoledgementPacket(CommPacket p) {
    //TODO:: Handle Later when figured out stuff

    return resolved;
}


void Scheduler_miniPC::serialPacketCB(CommPacket packet) {
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

