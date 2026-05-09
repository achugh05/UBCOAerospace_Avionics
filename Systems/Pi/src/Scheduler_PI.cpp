//
// Created by cline on 3/26/26.
//

#include "../include/Scheduler_PI.h"

Scheduler_PI::Scheduler_PI() {
    std::cout<<"Press enter to exit"<<std::endl;
    cfgRef = &cfg;
    init(false);
    initCameras();

    std::cin.sync();
    std::cin.get();
}

Scheduler_PI::~Scheduler_PI() {
    for (auto& cam: cameraStreamers) {
        cam->stop();
    }

    TCPconn->closeConn();
    UDPconn->closeConn();
    logger->close();
}

void Scheduler_PI::initCameras() {
    for (int i = 0; i < cfg.cameraConfig.devices.size(); ++i) {
        auto dev = cfg.cameraConfig.devices[i];
        auto vidLink = std::make_unique<VideoLink>(logger, cfg.networkConfig.localAddr,7000,
                                                            cfg.networkConfig.remoteAddr, 7001);
        cameraStreamers.push_back(std::make_unique<CameraStreamer>(
            CameraDevice{
                dev,
                cfg.cameraConfig.width,
                cfg.cameraConfig.height
            },
            std::move(vidLink),
            logger
            )
        );
    }

    for (auto& cam: cameraStreamers) {
        cam->prepareStream();
    }

    logger->println(VerbosityLevel::INFO, SubsystemTag::VIDEO, "Camera streaming initialized");
}

void Scheduler_PI::networkPacketTCP_CB(CommPacket packet) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Message Received by network (TCP): " + packet.toString());

    if (validatePacket(packet)) {
        switch (processPacket(packet)) {
            case passOn: {
                auto message = packet.toMessage();
                Serialconn->sendPacket(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Passing message on to: "+getSystemIdName(packet.SourceID));
            } break;
            case resolved : {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Processed message successfully");
            } break;
            case sendAcknowledge: {
                auto message = CommPacket::makeAcknowledgementPacket(miniPC, (SystemIDs) packet.SourceID).toMessage();
                TCPconn->sendData(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Sending acknowledgement packet to: "+getSystemIdName(packet.SourceID));
            } break;
            case failed: {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Failed to process packet: "+packet.toString());
            } break;
        }
    } else {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Packet received from source is malformed");
    }
}

void Scheduler_PI::networkPacketUDP_CB(CommPacket packet) {
}

void Scheduler_PI::serialPacketCB(CommPacket packet) {
    logger->println(VerbosityLevel::INFO, SubsystemTag::COMMS, "Test Json Received from serial: "+packet.toString());

    if (validatePacket(packet)) {
        switch (processPacket(packet)) {
            case passOn: {
                auto message = packet.toMessage();
                TCPconn->sendData(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Passing message on to: "+getSystemIdName(packet.SourceID));
            } break;
            case resolved : {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Processed message successfully");
            } break;
            case sendAcknowledge: {
                auto message = CommPacket::makeAcknowledgementPacket(miniPC, (SystemIDs) packet.SourceID).toMessage();
                Serialconn->sendPacket(message.data(), message.size());
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Sending acknowledgement packet to: "+getSystemIdName(packet.SourceID));
            } break;
            case failed: {
                logger->println(VerbosityLevel::DEBUG, SubsystemTag::STATION, "Failed to process packet: "+packet.toString());
            } break;
        }
    } else {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Packet received from source is malformed");
    }
}

void Scheduler_PI::nConnMadeCB() {
    logger->println(VerbosityLevel::INFO, SubsystemTag::NETWORK, "Successfully Connected (TCP)");
    for (auto& cam : cameraStreamers) {
        cam->startSending();
    }
}

void Scheduler_PI::nConnLostCB() {
    logger->println(VerbosityLevel::FAULT, SubsystemTag::NETWORK, "Unexpected disconnection occured, attempting recconnection (TCP)");

    for (auto& cam : cameraStreamers) {
        cam->pauseSending();
    }
}

commandResponseAction Scheduler_PI::processPacket(CommPacket p) {
    if (p.Destination_ID != pi_station) {
        return passOn;
    }
    //DO STUFF
    return resolved;
}
