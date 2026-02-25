#ifndef __ALERTSERVERAPP_H__
#define __ALERTSERVERAPP_H__

#include "inet/applications/udpapp/UdpBasicApp.h"
#include "inet/networklayer/common/L3AddressResolver.h"

using namespace inet;

class AlertServerApp : public UdpBasicApp {
protected:
    // Override the processPacket function to handle incoming packets
    virtual void processPacket(Packet *packet) override;
};

#endif