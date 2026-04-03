//
// Created by cline on 1/28/26.
//

#ifndef GSE_SCHEDULER_H
#define GSE_SCHEDULER_H

#include "configStructs.h"
#include "NetworkLink.h"
#include "Serial.h"

//Added this in case more response actions are needed in the future.
enum commandResponseAction {
    passOn,
    resolved,
    sendAcknowledge,
    failed
};

class Scheduler{

protected:
    systemConfig* cfgRef = nullptr;
    std::shared_ptr<Logger> logger;

    //networkstuff
    std::unique_ptr<NetworkLink> TCPconn;
    std::unique_ptr<NetworkLink> UDPconn;

    //std::unique_ptr<NetworkLink> test;

    std::unique_ptr<Serial> Serialconn;

public:
    virtual ~Scheduler()= default;


public:

    //Callbacks
    bool validatePacket(CommPacket p){ return cfgRef->validate();};

protected:
    void init(bool isServer);
private:

    void loadCFG();
    void initLogger();
    void initNetworkConnection(bool isServer);
    void initSerialConnection();

public:
    virtual void networkPacketTCP_CB(CommPacket packet) = 0;
    virtual void networkPacketUDP_CB(CommPacket packet) = 0;
    virtual void serialPacketCB(CommPacket packet) = 0;

    virtual void nConnMadeCB() = 0;
    virtual void nConnLostCB() = 0;

    virtual commandResponseAction processPacket(CommPacket p) = 0;
};


#endif //GSE_SCHEDULER_H
