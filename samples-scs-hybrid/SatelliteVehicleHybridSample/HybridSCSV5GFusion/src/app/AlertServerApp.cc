#include "AlertServerApp.h"
#include "../HybridInterfaceManager.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

Define_Module(AlertServerApp);

void AlertServerApp::processPacket(Packet *pk) {
    if (!pk) { return; } // Skip if no packet

    L3Address destAddr;    
    std::string interface = "wlan0";
    bool isAlertPacket = (std::string(pk->getName()).find("AlertTriggerPacket") == 0);
    
    // Check if is AlertPacket
    if (isAlertPacket) {
        // Get the destination port from parameters
        int broadcastPort = par("destPort").intValue();

        // Get the source port and source address from the packet's tags
        int srcPort = pk->getTag<L4PortInd>()->getSrcPort();
        L3Address srcAddr = pk->findTag<L3AddressInd>()->getSrcAddress();  
        std::cerr << "AlertServerApp: Received alert trigger from " << srcAddr << std::endl;
       
        cModule *network = getSimulation()->getSystemModule();
        if(!network) { return; } // Skip if no network module (should not happen in a well-formed simulation)

        for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
            cModule *submod = *it;
            if (std::string(submod->getName()).find("node") != 0) { continue; } // Skip non-node modules

            destAddr = L3Address(); // Reset destAddr for each node
            interface = "wlan0"; // Default to satellite interface

            if (submod->par("enableSwitching").boolValue()) {
                cModule *manager = submod->getSubmodule("interfaceManager");
                if (!manager) { continue; } // Skip if no interface manager (should not happen in a well-formed simulation)

                HybridInterfaceManager *hybridMgr = check_and_cast<HybridInterfaceManager*>(manager);
                NetworkInterface *activeIf = hybridMgr->getCurrentActiveInterface();
                if (!activeIf) { continue; } // Skip if no active interface

                auto *ipv4data = activeIf->findProtocolData<Ipv4InterfaceData>();
                if (!ipv4data || ipv4data->getIPAddress().isUnspecified()) { continue; } // Skip if no valid IP address on active interface

                destAddr = L3Address(ipv4data->getIPAddress()); // Get the IP address of the active interface
                interface = activeIf->getInterfaceName(); // Get the name of the active interface (e.g., "wlan0" or "cellular")
            } 
            else {
                // Case: SatelliteOnly
                try{
                    destAddr = L3AddressResolver().resolve((submod->getFullPath() + "\%wlan0").c_str());
                } catch (...){};
                // Case: CellularOnly
                if (destAddr.isUnspecified()) {
                    try{
                        destAddr = L3AddressResolver().resolve((submod->getFullPath() + "\%cellular").c_str());
                        interface = "cellular"; 
                    } catch (...){};
                }
            }

            if (destAddr.isUnspecified() || destAddr == srcAddr) { continue; } // Skip if no valid destination address or if the destination is the source

            Packet *fwdPacket = new Packet("AlertPacket");
            const auto& payload = makeShared<BytesChunk>(std::vector<uint8_t>(200, 0));
            fwdPacket->insertAtBack(payload);
            socket.sendTo(fwdPacket, destAddr, broadcastPort);

            std::cerr << "AlertServerApp: Send Alert to " << submod->getFullPath()
                << " on interface=" << interface 
                << " addr=" << destAddr 
            << std::endl;
        }
    }
    delete pk;
}