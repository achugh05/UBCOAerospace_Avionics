//
// Created by cline on 1/28/26.
//

#ifndef GSE_SCHEDULER_H
#define GSE_SCHEDULER_H

#include "configStructs.h"
#include "NetworkLink.h"
#include "Serial.h"
#include "DataStuctures/FootballConfig.h"
#include "DataStuctures/Subsystem.h"

//Added this in case more response actions are needed in the future.
enum commandResponseAction {
    passOn,
    resolved,
    sendAcknowledge,
    failed
};

class Scheduler {
    FootballStationConfig cfg;
    std::shared_ptr<Logger> logger;

    //networkstuff
    std::unique_ptr<NetworkLink> TCPconn;
    std::unique_ptr<NetworkLink> test;
    std::unique_ptr<NetworkLink> UDPconn;

    std::unique_ptr<Serial> Serialconn;

    //Management Stuff
    std::map<std::string, Subsystem> connectedSubsystems;

public:
    Scheduler();
    ~Scheduler();

private:
    void loadCFG();
    void initLogger();
    void initNetworkConnection();
    void initSerialConnection();

    void initConnectedSystems();
public:
    //Callbacks

    void networkPacketTCP_CB(CommPacket packet);
    void networkPacketUDP_CB(CommPacket packet);
    void serialPacketCB(CommPacket packet);

    void nConnMadeCB();
    void nConnLostCB();


    bool validatePacket(CommPacket p);
    commandResponseAction processPacket(CommPacket p);
private:
    commandResponseAction processCommandPacket(CommPacket p);
    commandResponseAction processTelemetryPacket(CommPacket p);
    commandResponseAction processErrorPacket(CommPacket p);
    commandResponseAction processAcknoledgementPacket(CommPacket p);
};


#endif //GSE_SCHEDULER_H
