#ifndef __ALERTSERVERAPP_H__
#define __ALERTSERVERAPP_H__

#include <queue>
#include "inet/applications/udpapp/UdpBasicApp.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/NetworkInterface.h"

using namespace inet;

struct PendingAlert {
    inet::L3Address destAddr;
    int             destPort;
    simtime_t       triggerCreationTime;
    int             sequenceNumber;
};

class AlertServerApp : public UdpBasicApp {

protected:
    virtual void processPacket(Packet *packet) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;

private:
    simtime_t extractTriggerCreationTime(Packet *pk);
    L3Address resolveNodeDestAddr(cModule *node, std::string &outInterface);
    void      broadcastAlert(simtime_t triggerCreationTime, const L3Address &srcAddr);
    void      sendNextPending();

    std::queue<PendingAlert> pendingAlerts;
    cMessage *sendTimer = nullptr;

    // Stagger interval between consecutive AlertPackets sent to different nodes.
    // Prevents MAC queue burst serialization on the satellite downlink.
    // At 150Mbps with 200B packets, each transmission takes ~10us;
    // 1ms stagger is ~100x the transmission time, ensuring the queue is drained
    // between consecutive sends.
    static constexpr double STAGGER_MS = 1.0;
};

#endif