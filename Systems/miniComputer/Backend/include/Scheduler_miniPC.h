//
// Created by cline on 1/28/26.
//

#ifndef GSE_SCHEDULER_MINIPC_H
#define GSE_SCHEDULER_MINIPC_H

#include "configStructs.h"
#include "NetworkLink.h"
#include "Serial.h"
#include "../../../../Libraries/SystemBase/include/Scheduler.h"
#include "DataStuctures/FootballConfig.h"
#include "DataStuctures/Subsystem.h"


class Scheduler_miniPC : Scheduler{
    FootballStationConfig cfg;

    //Management Stuff
    std::map<std::string, Subsystem> connectedSubsystems;

public:
    Scheduler_miniPC();
    ~Scheduler_miniPC() override;

private:
    void initConnectedSystems();
public:
    //Callbacks

    void networkPacketTCP_CB(CommPacket packet) override;
    void networkPacketUDP_CB(CommPacket packet) override;
    void serialPacketCB(CommPacket packet) override;

    void nConnMadeCB() override;
    void nConnLostCB() override;

    commandResponseAction processPacket(CommPacket p) override;
private:
    commandResponseAction processCommandPacket(CommPacket p);
    commandResponseAction processTelemetryPacket(CommPacket p);
    commandResponseAction processErrorPacket(CommPacket p);
    commandResponseAction processAcknoledgementPacket(CommPacket p);
};


#endif //GSE_SCHEDULER_H
