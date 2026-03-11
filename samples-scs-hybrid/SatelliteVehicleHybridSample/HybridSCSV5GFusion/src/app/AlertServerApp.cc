#include "AlertServerApp.h"
#include "../HybridInterfaceManager.h"

#include "inet/common/TimeTag_m.h"
#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

Define_Module(AlertServerApp);

// ============================================================================
// processPacket — entry point, delegates to broadcastAlert if trigger received
// ============================================================================

void AlertServerApp::processPacket(Packet *pk) {
    if (!pk) return;

    bool isTrigger = (std::string(pk->getName()).find("AlertTriggerPacket") == 0);
    if (isTrigger) {
        simtime_t triggerTime = extractTriggerCreationTime(pk);
        L3Address srcAddr = pk->findTag<L3AddressInd>()->getSrcAddress();
        EV_INFO << "AlertServerApp: received AlertTriggerPacket from " << srcAddr
                << " (t=" << triggerTime << ")\n";
        broadcastAlert(triggerTime, srcAddr);
    }

    UdpBasicApp::processPacket(pk);
}

// ============================================================================
// handleMessageWhenUp — intercepts the stagger self-message
// ============================================================================

void AlertServerApp::handleMessageWhenUp(cMessage *msg) {
    if (msg == sendTimer) {
        sendNextPending();
    } else {
        UdpBasicApp::handleMessageWhenUp(msg);
    }
}

// ============================================================================
// extractTriggerCreationTime — reads CreationTimeTag from ApplicationPacket chunk
// ============================================================================

simtime_t AlertServerApp::extractTriggerCreationTime(Packet *pk) {
    if (pk->hasAtFront<ApplicationPacket>()) {
        const auto& appPkt = pk->peekAtFront<ApplicationPacket>();
        if (appPkt) {
            const auto* tag = appPkt->findTag<CreationTimeTag>().get();
            if (tag)
                return tag->getCreationTime();
        }
    }
    EV_WARN << "AlertServerApp: CreationTimeTag not found, falling back to getCreationTime()\n";
    return pk->getCreationTime();
}

// ============================================================================
// broadcastAlert — collects all destinations into the pending queue,
//                  then starts draining via the stagger timer
// ============================================================================

void AlertServerApp::broadcastAlert(simtime_t triggerCreationTime, const L3Address &srcAddr) {
    cModule *network = getSimulation()->getSystemModule();
    if (!network) return;

    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *submod = *it;
        if (std::string(submod->getName()).find("node") != 0) continue;

        std::string interface;
        L3Address destAddr = resolveNodeDestAddr(submod, interface);
        if (destAddr.isUnspecified() || destAddr == srcAddr) continue;

        pendingAlerts.push({
                destAddr,
                (int)par("destPort").intValue(),
                triggerCreationTime,
                numSent}
        );
    }

    if (!sendTimer)
        sendTimer = new cMessage("alertSendTimer");

    if (!sendTimer->isScheduled())
        scheduleAt(simTime(), sendTimer);
}

// ============================================================================
// sendNextPending — sends one AlertPacket and re-schedules if queue is non-empty
// ============================================================================

void AlertServerApp::sendNextPending() {
    if (pendingAlerts.empty()) return;

    PendingAlert pending = pendingAlerts.front();
    pendingAlerts.pop();

    Packet *fwdPacket = new Packet("AlertPacket");
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(200));
    payload->setSequenceNumber(pending.sequenceNumber);
    payload->addTag<CreationTimeTag>()->setCreationTime(pending.triggerCreationTime);
    fwdPacket->insertAtBack(payload);

    emit(packetSentSignal, fwdPacket);
    socket.sendTo(fwdPacket, pending.destAddr, pending.destPort);
    numSent++;

    EV_INFO << "AlertServerApp: forwarded AlertPacket to " << pending.destAddr << "\n";

    if (!pendingAlerts.empty())
        scheduleAt(simTime() + SimTime(STAGGER_MS, SIMTIME_MS), sendTimer);
}

// ============================================================================
// resolveNodeDestAddr — returns the active IP address for a node
// ============================================================================

L3Address AlertServerApp::resolveNodeDestAddr(cModule *node, std::string &outInterface) {
    outInterface = "wlan0";
    L3Address destAddr;

    if (node->par("enableSwitching").boolValue()) {
        cModule *mgr = node->getSubmodule("interfaceManager");
        if (!mgr) return destAddr;

        HybridInterfaceManager *hybridMgr = check_and_cast<HybridInterfaceManager*>(mgr);
        NetworkInterface *activeIf = hybridMgr->getCurrentActiveInterface();
        if (!activeIf) return destAddr;

        auto *ipv4data = activeIf->findProtocolData<Ipv4InterfaceData>();
        if (!ipv4data || ipv4data->getIPAddress().isUnspecified()) return destAddr;

        outInterface = activeIf->getInterfaceName();
        destAddr = L3Address(ipv4data->getIPAddress());
    } else {
        try {
            destAddr = L3AddressResolver().resolve((node->getFullPath() + "%wlan0").c_str());
        } catch (...) {}

        if (destAddr.isUnspecified()) {
            try {
                destAddr = L3AddressResolver().resolve((node->getFullPath() + "%cellular").c_str());
                outInterface = "cellular";
            } catch (...) {}
        }
    }

    return destAddr;
}


/* #include "AlertServerApp.h"
#include "../HybridInterfaceManager.h"

#include "inet/common/TimeTag_m.h"
#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

Define_Module(AlertServerApp);


void AlertServerApp::processPacket(Packet *pk) {
    if (!pk) return;

    bool isTrigger = (std::string(pk->getName()).find("AlertTriggerPacket") == 0);
    if (isTrigger) {
        simtime_t triggerTime = extractTriggerCreationTime(pk);
        L3Address srcAddr = pk->findTag<L3AddressInd>()->getSrcAddress();
        EV_INFO << "AlertServerApp: received AlertTriggerPacket from " << srcAddr
                << " (t=" << triggerTime << ")\n";
        broadcastAlert(triggerTime, srcAddr);
    }

    UdpBasicApp::processPacket(pk);
}


simtime_t AlertServerApp::extractTriggerCreationTime(Packet *pk) {
    if (pk->hasAtFront<ApplicationPacket>()) {
        const auto& appPkt = pk->peekAtFront<ApplicationPacket>();
        if (appPkt) {
            const auto* tag = appPkt->findTag<CreationTimeTag>().get();
            if (tag)
                return tag->getCreationTime();
        }
    }
    EV_WARN << "AlertServerApp: CreationTimeTag not found, falling back to getCreationTime()\n";
    return pk->getCreationTime();
}


void AlertServerApp::broadcastAlert(simtime_t triggerCreationTime, const L3Address &srcAddr) {
    cModule *network = getSimulation()->getSystemModule();
    if (!network) return;

    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *submod = *it;
        if (std::string(submod->getName()).find("node") != 0) continue;
        forwardAlertTo(submod, triggerCreationTime, srcAddr);
    }
}

void AlertServerApp::forwardAlertTo(cModule *node, simtime_t triggerCreationTime, const L3Address &srcAddr) {
    std::string interface;
    L3Address destAddr = resolveNodeDestAddr(node, interface);

    if (destAddr.isUnspecified() || destAddr == srcAddr) return;

    Packet *fwdPacket = new Packet("AlertPacket");
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(200));
    payload->setSequenceNumber(numSent);
    payload->addTag<CreationTimeTag>()->setCreationTime(triggerCreationTime);
    fwdPacket->insertAtBack(payload);

    emit(packetSentSignal, fwdPacket);
    socket.sendTo(fwdPacket, destAddr, par("destPort").intValue());
    numSent++;

    EV_INFO << "AlertServerApp: forwarded AlertPacket to " << node->getFullPath()
            << " via " << interface << " (" << destAddr << ")\n";
}


L3Address AlertServerApp::resolveNodeDestAddr(cModule *node, std::string &outInterface) {
    outInterface = "wlan0";
    L3Address destAddr;

    if (node->par("enableSwitching").boolValue()) {
        cModule *mgr = node->getSubmodule("interfaceManager");
        if (!mgr) return destAddr;

        HybridInterfaceManager *hybridMgr = check_and_cast<HybridInterfaceManager*>(mgr);
        NetworkInterface *activeIf = hybridMgr->getCurrentActiveInterface();
        if (!activeIf) return destAddr;

        auto *ipv4data = activeIf->findProtocolData<Ipv4InterfaceData>();
        if (!ipv4data || ipv4data->getIPAddress().isUnspecified()) return destAddr;

        outInterface = activeIf->getInterfaceName();
        destAddr = L3Address(ipv4data->getIPAddress());
    } else {
        // SatelliteOnly
        try {
            destAddr = L3AddressResolver().resolve((node->getFullPath() + "%wlan0").c_str());
        } catch (...) {}

        // CellularOnly fallback
        if (destAddr.isUnspecified()) {
            try {
                destAddr = L3AddressResolver().resolve((node->getFullPath() + "%cellular").c_str());
                outInterface = "cellular";
            } catch (...) {}
        }
    }

    return destAddr;
} */