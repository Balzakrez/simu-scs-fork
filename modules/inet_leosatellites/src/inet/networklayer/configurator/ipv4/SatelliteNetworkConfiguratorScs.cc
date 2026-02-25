//
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
// Copyright (C) 2026 Giuseppe Balzano
//
// SPDX-License-Identifier: LGPL-3.0-or-later
// 
// This program was developed based on INET Framework (https://inet.omnetpp.org/).
// The original code can be found at `inet/networklayer/configurator/ipv4/L3NetworkConfiguratorBase.cc` of INET-4.4.1.

#include "SatelliteNetworkConfiguratorScs.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "veins_inet_scs/VeinsInetMobility.h" // from scs_optional
#include "scs_utils/converter/PositionConverter.h" // from scs_utils
#include "leosatellites/mobility/NoradA.h"
#include "leosatellites/mobility/SatelliteMobility.h"
#include "leosatellites/mobility/GroundStationMobility.h"
#include "leosatellites/networklayer/configurator/ipv4/MatcherOS3.h"
#include "scs/common/beamInfo/BeamInfo.h" // from scs
#include "common/binder/Binder.h" // from Simu5G
#include "stack/mac/layer/LteMacBase.h" // from Simu5G

#define VERBOSE_LOGGING 0

Define_Module(inet::SatelliteNetworkConfiguratorScs);

namespace inet {

    SatelliteNetworkConfiguratorScs::~SatelliteNetworkConfiguratorScs() {
        delete this->wlanPoolPtr;
        delete this->cellularPoolPtr;
    }

    static double parseCostAttribute(const char *costAttribute) {
        if (!strncmp(costAttribute, "inf", 3))
            return INFINITY;
        else {
            double cost = atof(costAttribute);
            if (cost <= 0)
                throw cRuntimeError("Cost cannot be less than or equal to zero");
            return cost;
        }
    }

    void SatelliteNetworkConfiguratorScs::initialize(int stage) {   
        // Call base class initialize
        SatelliteNetworkConfigurator::initialize(stage);
        
        if (stage == INITSTAGE_LOCAL) {
            // Setup wlanPool for wlan0: 10.3.0.0/24 (satellite)
            this->wlanPoolPtr = new IpPool(Ipv4Address(10,3,0,0), Ipv4Address(255,255,255,0));

            // Setup cellularPool for cellular: 10.4.0.0/24 (5G)
            this->cellularPoolPtr = new IpPool(Ipv4Address(10,4,0,0), Ipv4Address(255,255,255,0));

            // Cache PositionConverter to avoid repeated lookups
            this->posConverterPtr = check_and_cast<Satellite::PositionConverter*>(getSimulation()->getSystemModule()->getSubmodule("Pos"));
        }
    }

    Ipv4Address SatelliteNetworkConfiguratorScs::allocateIpFromPool(IpPool* poolPtr, int moduleId, std::map<int, uint32_t>& nodeToIpMap){
        // Check if the node already has an assigned IP
        auto it = nodeToIpMap.find(moduleId);
        if (it != nodeToIpMap.end()) {
            return Ipv4Address(it->second);
        }
        uint32_t newIp;
        // 1. Try to reuse freed addresses first
        if (!poolPtr->freeAddressesQueue.empty()){
            newIp = poolPtr->freeAddressesQueue.front();
            poolPtr->freeAddressesQueue.pop();
        }
        else { 
            // 2. Otherwise allocate a new IP
            if (poolPtr->nextAddress > poolPtr->maxHosts){
                throw cRuntimeError("IP pool exhausted! No more addresses available in subnet");
            }
            uint32_t hostPart = poolPtr->nextAddress++; // Increment for next allocation
            newIp = poolPtr->baseAddress | hostPart; // Combine network and host parts
        }
        // 3. Mark the new IP as used and map to node
        poolPtr->usedAddressesSet.insert(newIp);
        nodeToIpMap[moduleId] = newIp;
        return Ipv4Address(newIp);
    }

    void SatelliteNetworkConfiguratorScs::releaseIpToPool(IpPool* poolPtr, int moduleId, std::map<int, uint32_t>& nodeToIpMap) {
        // Check if the node has an assigned IP
        auto it = nodeToIpMap.find(moduleId);
        if (it == nodeToIpMap.end()) {
            return; 
        }
        uint32_t ipToFree = it->second;
        // 1. Remove from used set
        poolPtr->usedAddressesSet.erase(ipToFree);
        // 2. Add to free queue for reuse
        poolPtr->freeAddressesQueue.push(ipToFree);
        // 3. Remove from mapping
        nodeToIpMap.erase(it);
    }

    void SatelliteNetworkConfiguratorScs::cleanupVeinsNode() {       
        // Set for collecting unique Veins module IDs 
        std::set<int> veinsModuleIdSet;
        // 1. Collect all registered Veins module IDs from cellular map
        for(auto& entry : this->nodeToCellularIpMap) {
            veinsModuleIdSet.insert(entry.first);
        }
        // 2. Collect all registered Veins module IDs from wlan map
        for(auto& entry : this->nodeToWlanIpMap) {
            veinsModuleIdSet.insert(entry.first);
        }
        // Check each veins node for validity
        for (auto it = veinsModuleIdSet.begin(); it != veinsModuleIdSet.end();) {
            int moduleId = *it;
            cModule* currNodeModule = getSimulation()->getModule(moduleId);
            // If module is deleted or not a valid Veins node anymore
            if (currNodeModule == nullptr || !isVeinsNode(currNodeModule)) {
                unRegisterFromBinder(moduleId); // Module already destroyed, not needed
                releaseIpToPool(this->wlanPoolPtr, moduleId, this->nodeToWlanIpMap);
                releaseIpToPool(this->cellularPoolPtr, moduleId, this->nodeToCellularIpMap);
                it = veinsModuleIdSet.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    MacNodeId SatelliteNetworkConfiguratorScs::findMacNodeId(cModule* host, const Ipv4Address& cellularIp) {
        if (!host) return 0;
        
        Binder* binder = check_and_cast<Binder*>(getSimulation()->getModuleByPath("binder"));
        MacNodeId macNodeId = 0;
        
        // Method 1: Try to find MacNodeId using cellularNic module
        cModule* cellularNic = host->getSubmodule("cellularNic");
        if (cellularNic) {
            cModule* macModule = cellularNic->getSubmodule("mac");
            if (macModule) {
                macNodeId = binder->getMacNodeIdFromOmnetId(macModule->getId());
                if (macNodeId > 0) {
                    return macNodeId;
                }
            }
        }
        // Method 2: Try to find MacNodeId from module parameter
        if (host->hasPar("macNodeId")) {
            macNodeId = host->par("macNodeId").intValue();
            if (macNodeId > 0) {
                return macNodeId;
            }
        }
        // Method 3: Try to find MacNodeId using IP (might already be mapped)
        if (!cellularIp.isUnspecified()) {
            macNodeId = binder->getMacNodeId(cellularIp);
            if (macNodeId > 0) {
                return macNodeId;
            }
        }
        // Method 4: Try cached mapping
        auto it = nodeIdToMacNodeIdMap.find(host->getId());
        if (it != nodeIdToMacNodeIdMap.end()) {
            macNodeId = it->second;
            if (macNodeId > 0) {
                return macNodeId;
            }
        }
        return 0;
    }

    void SatelliteNetworkConfiguratorScs::registerToBinder(const Ipv4Address newAddr, cModule* host) {
        try {
            Binder* binder = check_and_cast<Binder*>(getSimulation()->getModuleByPath("binder"));
            // Check if already registered 
            MacNodeId currMacNodeId = findMacNodeId(host, newAddr); 
            // Method 4: Manual registration (last fallback)
            if (currMacNodeId == 0) {
                int masterId = host->hasPar("masterId") ? host->par("masterId").intValue() : 1;
                int cellId = host->hasPar("macCellId") ? host->par("macCellId").intValue() : 1;
                currMacNodeId = binder->registerNode(host, UE, masterId, false);
                binder->updateUeInfoCellId(currMacNodeId, cellId);
            }
            // Create/Update IP -> MacNodeId mapping
            if (currMacNodeId > 0) {
                EV_INFO << "Binder mapping: Host=" << host->getFullPath() 
                        << " with IP=" << newAddr 
                        << " is already mapped to MacNodeId=" << currMacNodeId 
                        << std::endl;
                MacNodeId mappedId = binder->getMacNodeId(newAddr);
                if (mappedId == 0) {
                    binder->setMacNodeId(newAddr, currMacNodeId);
                    EV_INFO << "Binder mapping: IP=" << newAddr 
                            << " mapped to MacNodeId=" << currMacNodeId 
                            << std::endl;
                }
                else if (mappedId != currMacNodeId) {
                    EV_INFO << "Binder mapping conflict: IP=" << newAddr 
                            << " already mapped to MacNodeId=" << mappedId 
                            << ", cannot map to MacNodeId=" << currMacNodeId 
                            << std::endl;
                }
            }
            nodeIdToMacNodeIdMap[host->getId()] = currMacNodeId; // Cache mapping
            EV_INFO << "Binder registration successful for host " << host->getFullPath() 
                    << " with IP " << newAddr 
                    << " and MacNodeId " << currMacNodeId 
                    << std::endl;

        } catch (std::exception& e) {
            throw cRuntimeError("Error during Binder registration for %s: %s", 
                            host->getFullPath().c_str(), e.what());
        }
    }

    void SatelliteNetworkConfiguratorScs::unRegisterFromBinder(int moduleId) {
        auto itCell = nodeToCellularIpMap.find(moduleId);
        if (itCell == nodeToCellularIpMap.end()) { return; }
        
        Ipv4Address currCellIp = Ipv4Address(itCell->second);
        cModule* host = getSimulation()->getModule(moduleId);
        Binder* binder = check_and_cast<Binder*>(getSimulation()->getModuleByPath("binder"));
        MacNodeId currMacId = findMacNodeId(host, currCellIp); // Try to find MacNodeId using multiple methods
        // Unregister if found
        if (currMacId > 0) {
            binder->setMacNodeId(currCellIp, 0); // Clear mapping
            binder->unregisterNode(currMacId); // Unregister node
            nodeIdToMacNodeIdMap.erase(moduleId); // Remove from cache
            EV_INFO << "Binder unregistration: Host=" << host->getFullPath() 
                    << " with IP=" << currCellIp 
                    << " and MacNodeId=" << currMacId 
                    << " has been unregistered" 
                    << std::endl;
        }
    }
    
    void SatelliteNetworkConfiguratorScs::checkAndConfigureVehicleInterfaces() {
        // Clean up exited vehicles first
        cleanupVeinsNode(); 
        // Configure interfaces for all Veins nodes
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            if (!isVeinsNode(node->getModule())) { continue; } // Skip non-Veins nodes
            cModule* host = node->getModule();
            Ipv4RoutingTable *rt = dynamic_cast<Ipv4RoutingTable*>(node->routingTable);
            if (!rt) { throw cRuntimeError("Node %s has no Ipv4RoutingTable", host->getFullPath().c_str()); }

            for (auto& entry : node->interfaceInfos) {
                InterfaceInfo *info = static_cast<InterfaceInfo*>(entry);
                if (!info->networkInterface) { continue; } // Skip if no network interface
                
                Ipv4InterfaceData *ipv4Data = info->networkInterface->getProtocolDataForUpdate<Ipv4InterfaceData>();
                if (!ipv4Data) { continue; } // Skip if no IPv4 data
                
                std::string ifName = info->networkInterface->getInterfaceName();
                bool isCellular = (ifName == "cellular");
                bool isWlan = (ifName == "wlan0");

                if (!isCellular && !isWlan) { continue; } // Skip non-target interfaces

                // 1. Assign IP if not already assigned
                if (ipv4Data->getIPAddress().isUnspecified()) {
                    IpPool *currPoolPtr = isWlan ? this->wlanPoolPtr : this->cellularPoolPtr;
                    std::map<int, uint32_t> &nodeToInterfaceMapRef = isWlan ? this->nodeToWlanIpMap : this->nodeToCellularIpMap;
                    Ipv4Address newAddr = allocateIpFromPool(currPoolPtr, host->getId(), nodeToInterfaceMapRef);
                    ipv4Data->setIPAddress(newAddr);
                    ipv4Data->setNetmask(Ipv4Address(255, 255, 255, 0));
                }
                Ipv4Address currAddrIp = ipv4Data->getIPAddress();
                Ipv4Address netmaskAddr = ipv4Data->getNetmask();
                // 2. Register to Binder if Cellular interface
                if (isCellular) {
                    Binder* binder = check_and_cast<Binder*>(getSimulation()->getModuleByPath("binder"));
                    if(binder->getMacNodeId(currAddrIp) == 0){  // Call register only if not already registered
                        registerToBinder(currAddrIp, host);
                    }
                }
                // 3. Synchronize internal structures
                info->address = currAddrIp.getInt();
                info->addressSpecifiedBits = Ipv4Address::ALLONES_ADDRESS.getInt();
                info->netmask = netmaskAddr.getInt();
                info->netmaskSpecifiedBits = Ipv4Address::ALLONES_ADDRESS.getInt();
                // Update routing table
                // Note: With carrier-only switching, interfaces are always UP
                // Routes are added/enabled based on carrier state
                if(info->networkInterface->isUp() && info->networkInterface->hasCarrier()){
                    // Add direct route if not exists
                    Ipv4Address networkAddrIp = currAddrIp.doAnd(netmaskAddr);
                    if (!rt->findBestMatchingRoute(networkAddrIp)) {
                        Ipv4Route *directRoute = new Ipv4Route();
                        directRoute->setDestination(networkAddrIp);
                        directRoute->setNetmask(netmaskAddr);
                        directRoute->setInterface(info->networkInterface);
                        directRoute->setSourceType(Ipv4Route::MANUAL);
                        directRoute->setMetric(1); // Direct routes have lowest metric
                        rt->addRoute(directRoute);
                    }
                    // 5. Ensure default route via this interface if not exists
                    if(!rt->findBestMatchingRoute(Ipv4Address::UNSPECIFIED_ADDRESS)) {
                        Ipv4Route *defaultRoute = new Ipv4Route();
                        defaultRoute->setDestination(Ipv4Address::UNSPECIFIED_ADDRESS);
                        defaultRoute->setNetmask(Ipv4Address::UNSPECIFIED_ADDRESS);
                        defaultRoute->setGateway(Ipv4Address::UNSPECIFIED_ADDRESS); 
                        defaultRoute->setInterface(info->networkInterface);
                        defaultRoute->setSourceType(Ipv4Route::MANUAL);
                        defaultRoute->setMetric(1);
                        rt->addRoute(defaultRoute);
                    }
                }
            }
        }
    }

    void SatelliteNetworkConfiguratorScs::dumpIpPools() {
        std::cerr << "\n=== IP Pool Status ===" << endl;
        std::cerr << "WLAN Pool : " << this->wlanPoolPtr->baseAddress << endl;
        std::cerr << "  Used: " << wlanPoolPtr->usedAddressesSet.size() << " addresses" << endl;
        std::cerr << "  Free: " << wlanPoolPtr->freeAddressesQueue.size() << " addresses" << endl;
        std::cerr << "  Next: " << Ipv4Address(wlanPoolPtr->baseAddress | wlanPoolPtr->nextAddress) << endl;
        
        std::cerr << "Cellular Pool : " << this->cellularPoolPtr->baseAddress << endl;
        std::cerr << "  Used: " << cellularPoolPtr->usedAddressesSet.size() << " addresses" << endl;
        std::cerr << "  Free: " << cellularPoolPtr->freeAddressesQueue.size() << " addresses" << endl;
        std::cerr << "  Next: " << Ipv4Address(cellularPoolPtr->baseAddress | cellularPoolPtr->nextAddress) << endl;
        std::cerr << "=====================\n" << endl;
    }

    void SatelliteNetworkConfiguratorScs::handleMessage(cMessage *msg) {    
        if (msg == timer) {
            cXMLElementList autorouteElements = configuration->getChildrenByTagName("autoroute");
            if (autorouteElements.size() == 0) {
                cXMLElement defaultAutorouteElement("autoroute", "", nullptr);
                reinvokeConfigurator(topology, &defaultAutorouteElement);
            }
            else {
                for (auto & autorouteElement : autorouteElements){
                    reinvokeConfigurator(topology, autorouteElement);
                }
            }
            scheduleAt(simTime() + timerInterval, timer);
        }
    }

    bool SatelliteNetworkConfiguratorScs::isVeinsNode(cModule *mod){
        if (mod == nullptr) { return false; }
        if (mod->getSubmodule("mobility") == nullptr) { return false; }   
        return (dynamic_cast<veins::VeinsInetMobility*>(mod->getSubmodule("mobility"))) != nullptr;
    }

    bool SatelliteNetworkConfiguratorScs::isValidVeinsPosition(const inet::Coord& pos) {
        // Check for NaN or infinity
        if (std::isnan(pos.x) || std::isnan(pos.y) || std::isinf(pos.x) || std::isinf(pos.y)) {
            return false;
        }
        // Veins uses positive local coordinates (e.g., 0-50000)
        if (pos.x < 0 || pos.y < 0 || pos.x > 1e12 || pos.y > 1e12) {
            return false;
        }
        // Check altitude if present
        if (!std::isnan(pos.z) && (pos.z < -1000 || pos.z > 100000)) {
            return false;
        }
        return true;
    }

    bool SatelliteNetworkConfiguratorScs::isToExcludeLink(Link *link) {
            // Exclude links with interfaces that don't have carrier (physical connectivity)
            // Note: isUp() check is kept for robustness, but with carrier-only switching
            // all vehicle interfaces are always UP
            if (link->sourceInterfaceInfo && link->sourceInterfaceInfo->networkInterface) {
                auto *srcIf = link->sourceInterfaceInfo->networkInterface;
                if (!srcIf->isUp() || !srcIf->hasCarrier()) { return true; }
            }
            if (link->destinationInterfaceInfo && link->destinationInterfaceInfo->networkInterface) {
                auto *dstIf = link->destinationInterfaceInfo->networkInterface;
                if (!dstIf->isUp() || !dstIf->hasCarrier()) { return true; }
            }
            return false;
        }

    void SatelliteNetworkConfiguratorScs::printRoutingTable(){
        // Print routing table for all Veins nodes for debugging
        for (int i = 0; i < this->topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            if (!isVeinsNode(node->getModule())) { continue; } // Only show vehicle nodes
            if (node->getModule() == nullptr){ continue; } // Skip nodes without modules (should not happen)
            Ipv4RoutingTable *routingTable = dynamic_cast<Ipv4RoutingTable*>(node->routingTable);
            if (routingTable == nullptr){ continue; } // Skip nodes without IPv4 routing table (should not happen)
            std::string nodeName = node->getModule()->getFullPath();
            std::cerr << " Node " << nodeName << " have these routes: " << std::endl;
            if (routingTable->getNumRoutes() == 0) {
                std::cerr << "  (no routes)" << std::endl;
            }
            for (int i = 0; i < routingTable->getNumRoutes(); i++){
                Ipv4Route *route = routingTable->getRoute(i);
                std::cerr << "  Route[" << i << "]: " 
                        << " source=" << nodeName << " "
                        << " dest=" << route->getDestination() << "/netlen=" << route->getNetmask().getNetmaskLength()
                        << " netmask=" << route->getNetmask()
                        << " via " << route->getInterface()->getInterfaceName()
                        << " gw=" << route->getGateway()
                        << endl;
            }
        }
    }

    void SatelliteNetworkConfiguratorScs::filterVeinsSatelliteLinks(Topology& topology) {
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            if (!isVeinsNode(node->getModule())) continue; // Skip non-Veins nodes

            // Iterate over outgoing links of the Veins node
            veins::VeinsInetMobility* veinsMob = check_and_cast<veins::VeinsInetMobility*>(node->getModule()->getSubmodule("mobility"));
            inet::Coord vehPos = veinsMob->getCurrentPosition();
            
            // Early check for valid position to avoid unnecessary satellite link checks
            if(!isValidVeinsPosition(vehPos)) { continue; }
            
            // Cache vehicle latitude and longitude for this node to avoid repeated conversions in the loop
            double vehLat = posConverterPtr->convertPosYToLatitude(vehPos.y);
            double vehLon = posConverterPtr->convertPosXToLongitude(vehPos.x);
            
            for (int j = 0; j < node->getNumOutLinks(); j++) {
                Link *link = (Link *)node->getLinkOut(j);
                // Check if the link is to a satellite node and retrieve satellite mobility
                Node *remoteNode = (Node *)link->getLinkOutRemoteNode();
                cModule *remoteModule = remoteNode->getModule();
                SatelliteMobility *satMobility = dynamic_cast<SatelliteMobility*>(remoteModule->getSubmodule("mobility"));
                // Retrieve BeamInfo module
                // Satellite::BeamInfo *beamInfo = dynamic_cast<Satellite::BeamInfo*>(remoteModule->getSubmodule("bm")); 
                if (satMobility) {
                    double elevation = satMobility->getElevation(vehLat, vehLon, 0.0);
                    // Disable link if vehicle is outside beam coverage or not reachable
                    if(!satMobility->isReachable(vehLat, vehLon, 0.0)) {
                        link->disable();
                        // Inet manages links as directional, we need to find the corresponding reverse link
                        for (int k = 0; k < remoteNode->getNumOutLinks(); k++) {
                            Link *backLink = (Link *)remoteNode->getLinkOut(k);
                            if (backLink->getLinkOutRemoteNode() == node) {
                                backLink->disable(); // Disable the reverse link as well
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    void SatelliteNetworkConfiguratorScs::optimizeVehiclesRoutes(){
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i); 
            // Skip non-Veins nodes
            if (!isVeinsNode(node->getModule())) { continue; } 
            
            // Identify the active interface (up and has carrier)
            NetworkInterface* activeIface = nullptr;
            for (auto infoPtr : node->interfaceInfos) {
                InterfaceInfo* info = static_cast<InterfaceInfo*>(infoPtr);
                if (info->networkInterface && info->networkInterface->hasCarrier()) {
                    activeIface = info->networkInterface;
                    break; 
                }
            }
            
            // Skip if no active interface found
            if (!activeIface) { continue; } 
            // Identify the best gateway from existing routes on the active interface
            Ipv4Address bestGateway = Ipv4Address::UNSPECIFIED_ADDRESS;
            for (auto route : node->staticRoutes) {
                if (route->getInterface() == activeIface && !route->getGateway().isUnspecified()) {
                    bestGateway = route->getGateway();
                    break; // Take the first valid gateway found
                }
            }
            
            // Iterate and decide what to keep and what to discard
            for (auto it = node->staticRoutes.begin(); it != node->staticRoutes.end();) {
                Ipv4Route* route = *it;
                // 1. Delete routes via inactive interfaces
                if (route->getInterface() != activeIface) {
                    delete route;
                    it = node->staticRoutes.erase(it);
                    continue;
                }
                // 2. Keep all routes (specific, default, direct) via active interface
                // if (!route->getGateway().isUnspecified() && bestGateway.isUnspecified()) {
                //     bestGateway = route->getGateway();  // Save best gateway
                // }
                // ++it;  
                // 3. Keep direct routes (to directly connected networks)
                if (route->getGateway().isUnspecified()) {
                    ++it;
                    continue;
                }
                // 4. Keep default routes (0.0.0.0)
                if (route->getDestination().isUnspecified()) {
                    // If gateway is unspecified, but we have a bestGateway, update it
                    if (route->getGateway().isUnspecified() && !bestGateway.isUnspecified()) {
                        route->setGateway(bestGateway);
                    }
                    ++it;
                    continue;
                }
                
                // 5. Delete specific routes via active interface because they are covered by default route
                delete route;
                it = node->staticRoutes.erase(it);
            }
            // DEFAULT ROUTE GUARANTEE 
            // If we have filtered specific routes, we must ensure that a Default Route exists
            bool hasDefaultRoute = false;
            // Check if a default route already exists
            for (auto route : node->staticRoutes) {
                if (route->getDestination().isUnspecified()) {
                    hasDefaultRoute = true;
                    break;
                }
            }
            // If no default route exists, add one via the best gateway
            if (!hasDefaultRoute && !bestGateway.isUnspecified()) {
                Ipv4Route *defaultRoute = new Ipv4Route();
                defaultRoute->setDestination(Ipv4Address::UNSPECIFIED_ADDRESS);
                defaultRoute->setNetmask(Ipv4Address::UNSPECIFIED_ADDRESS);
                defaultRoute->setGateway(bestGateway);
                defaultRoute->setInterface(activeIface);
                defaultRoute->setSourceType(Ipv4Route::MANUAL);
                defaultRoute->setMetric(1); // Default routes have lowest metric
                node->staticRoutes.push_back(defaultRoute);
            }
        }
    }

    void SatelliteNetworkConfiguratorScs::reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement) {
        // std::cerr << "\nReinvoking SatelliteNetworkConfiguratorScs at " << simTime() << endl;
        // std::cerr << "BEFORE CLEARING ROUTING TABLE:" << std::endl;
        // printRoutingTable();

        // 1. Clean routing tables and topology
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            node->interfaceInfos.clear();
            if (node->getModule() == nullptr){ continue; } // Skip nodes without modules (should not happen)
            Ipv4RoutingTable *routingTable = dynamic_cast<Ipv4RoutingTable*>(node->routingTable);
            // 1.1 Clear routing table (but preserve the cellular default route for vehicles)
            for(int j = routingTable->getNumRoutes() - 1; j >= 0; j--) {
                Ipv4Route *route = routingTable->getRoute(j);
                routingTable->deleteRoute(route);
            } 
            // 1.2 Clear multicast routing table
            for(int m = 0; m < node->routingTable->getNumMulticastRoutes(); m++) {
                node->routingTable->deleteMulticastRoute(node->routingTable->getMulticastRoute(m));
            }
            // 1.3 Clear static routes
            std::for_each(node->staticRoutes.begin(), node->staticRoutes.end(), []( Ipv4Route* route) { delete route; });
            node->staticRoutes.clear();
        } 
        // Clear links and interfaces from topology
        std::for_each(topology.linkInfos.begin(), topology.linkInfos.end(), []( LinkInfo* link) { delete link; });
        for(auto & p : topology.interfaceInfos) delete p.second;
        topology.linkInfos.clear();
        topology.interfaceInfos.clear();
        topology.clear();
            
        // 2. Re-extract topology
        SatelliteNetworkConfigurator::extractTopology(topology);

        // 3. Configure Veins vehicle interfaces (cellular, wlan0)
        // Both interfaces are always UP, IP assignment and Binder registration
        checkAndConfigureVehicleInterfaces();

        // 4. Filter satellite links based coverage/elevation
        filterVeinsSatelliteLinks(topology); 

        // 5. Recalculate routing
        SatelliteNetworkConfigurator::addStaticRoutes(topology, autorouteElement);

        // 6. Optimize vehicle routes (keep only routes via active interface)
        // Active = hasCarrier() since interfaces are always UP
        optimizeVehiclesRoutes();
       
        // 7. Configure all routing tables
        SatelliteNetworkConfigurator::configureAllRoutingTables();

        // std::cerr << "AFTER REINVOKING ROUTING TABLE:" << std::endl;
        // printRoutingTable();

        // Debug output
        if (par("dumpTopology").boolValue())
            dumpTopology(topology);

        if (par("dumpConfig").stringValue()[0])
            dumpConfiguration();
        
        if (par("dumpIpPools").boolValue())
            dumpIpPools();
    }

    double SatelliteNetworkConfiguratorScs::calculatePropagationDelay(double distanceKm) {
        if (std::isnan(distanceKm) || distanceKm < 0) { return INFINITY; }
        // Speed of light: 299,792,458 m/s
        return (distanceKm * 1000.0) / 299792458.0;
    }

    bool SatelliteNetworkConfiguratorScs::checkAndGetVehicleGeoCoordinates(veins::VeinsInetMobility* veinsMob, double& outLat, double& outLon) {
        if (!veinsMob || !veinsMob->getParentModule() || veinsMob->isTerminated()) {
            return false;
        }
        inet::Coord pos = veinsMob->getCurrentPosition();
        if (!isValidVeinsPosition(pos)) {
            return false;
        }
        outLon = posConverterPtr->convertPosXToLongitude(pos.x);
        outLat = posConverterPtr->convertPosYToLatitude(pos.y);
        return true;
    }

    double SatelliteNetworkConfiguratorScs::computeWirelessLinkWeight(Link *link, const char *metric, cXMLElement *parameters) {
        

        //Exclude links with interfaces that are down or have no carrier
        if (isToExcludeLink(link)) { return INFINITY; }

        const char *costAttribute = parameters->getAttribute("cost");
        if (costAttribute != nullptr)
            return parseCostAttribute(costAttribute);
        

        if (!strcmp(metric, "hopCount"))
            return 1;
            
        // Calculate propagation delay (used to determine if link exists)
        if (!strcmp(metric, "propagationDelay")) {
            
            if (!link->sourceInterfaceInfo || !link->destinationInterfaceInfo) { return INFINITY; }
            if (!link->sourceInterfaceInfo->node || !link->destinationInterfaceInfo->node) { return INFINITY; }

            // Get transmitter and receiver modules
            cModule *transmitterModule = link->sourceInterfaceInfo->node->module;
            cModule *receiverModule = link->destinationInterfaceInfo->node->module;
            
            // Verify modules are valid and not deleted
            if (!transmitterModule || !receiverModule) { return INFINITY; }

            // Get mobility modules
            cModule* txMob = transmitterModule->getSubmodule("mobility");
            cModule* rxMob = receiverModule->getSubmodule("mobility");

            if(!txMob || !rxMob) { return INFINITY; }
            // Ensure PositionConverter is available
            if(!posConverterPtr) { return INFINITY; }

            double delay = 0.0;
            // ========================================
            // CASE 1: Satellite -> X \in {Vehicle, GroundStation, Satellite}
            // ========================================
            if(SatelliteMobility *sourceSat = dynamic_cast<SatelliteMobility*>(txMob)) {
                // ========================================
                // Case 1.A: Satellite -> Vehicle (Veins)
                // ========================================
                if(veins::VeinsInetMobility *destVeins = dynamic_cast<veins::VeinsInetMobility*>(rxMob)) { 
                    // Check that the Veins vehicle module is still valid
                    if(!destVeins->getParentModule() || destVeins->isTerminated()) {
                        return INFINITY;
                    }
                    double vehLon, vehLat;
                    // Convert X,Y -> Lat,Lon using PositionConverter
                    if (!checkAndGetVehicleGeoCoordinates(destVeins, vehLat, vehLon)) {
                        return INFINITY;
                    }
                    if (!sourceSat->isReachable(vehLat, vehLon, 0.0)) {   
                        return INFINITY;
                    }
                    // Calculate distance Satellite <-> Vehicle (in km)
                    double distKm = sourceSat->getDistance(vehLat, vehLon, 0.0);
                    if(std::isnan(distKm) || distKm < 0) { 
                        return INFINITY;
                    }
                    // Calculate propagation delay (distance / speed of light)
                    delay = calculatePropagationDelay(distKm);
                    return delay; 
                }
                // ========================================
                // CASE 1.B: Satellite -> GroundStation
                // ========================================
                else if(GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {
                    // Calculate distance Satellite <-> GroundStation (in km)
                    double distKm = sourceSat->getDistance(destGS->getLUTPositionY(), destGS->getLUTPositionX(), 0.0);
                    if(std::isnan(distKm) || distKm < 0) {
                        return INFINITY;
                    }
                    // Calculate propagation delay (distance / speed of light)
                    delay = calculatePropagationDelay(distKm);
                    return delay;
                }
                // ========================================
                // CASE 1.C: Satellite -> Satellite 
                // ========================================
                else if(SatelliteMobility *destSat = dynamic_cast<SatelliteMobility*>(rxMob)) {
                    // Calculate distance Satellite <-> Satellite (in km)
                    double distKm = sourceSat->getDistance(destSat->getLatitude(), destSat->getLongitude(), destSat->getAltitude());
                    if(std::isnan(distKm) || distKm < 0) {  
                        return INFINITY;
                    }
                    // Calculate propagation delay (distance / speed of light)
                    delay = calculatePropagationDelay(distKm); 
                    return delay;
                }
            }
            // ========================================
            // CASE 2: Vehicle (Veins) -> X \in {Satellite, GroundStation, Vehicle}
            // ========================================
            else if(veins::VeinsInetMobility *sourceVeins = dynamic_cast<veins::VeinsInetMobility*>(txMob)) 
            {
                // Check that the Veins vehicle module is still valid
                if(!sourceVeins->getParentModule() || sourceVeins->isTerminated()) {
                    return INFINITY;
                }
                // ========================================
                // Case 2.A: Vehicle -> Satellite
                // ========================================
                if(SatelliteMobility *destSat = dynamic_cast<SatelliteMobility*>(rxMob)) {
                    // Check and get vehicle latitude and longitude
                    // Convert X,Y -> Lat,Lon using PositionConverter
                    double vehLon, vehLat;
                    if (!checkAndGetVehicleGeoCoordinates(sourceVeins, vehLat, vehLon)) {  
                        return INFINITY;
                    }
                    if (!destSat->isReachable(vehLat, vehLon, 0.0)) {   
                        return INFINITY;
                    }

                    // Calculate distance Satellite <-> Vehicle (in km)
                    double distKm = destSat->getDistance(vehLat, vehLon, 0.0);
                    if(std::isnan(distKm) || distKm < 0) { 
                        return INFINITY;
                    }
                    // Calculate propagation delay (distance / speed of light)
                    delay = calculatePropagationDelay(distKm);
                    return delay;
                }
                // ========================================
                // Case 2.B: Vehicle -> GroundStation (not implemented because not needed)
                // Use Vehicle->Satellite->GroundStation links instead
                // This method is used for wireless links only between satellite and ground nodes (GS or Veins vehicles)
                // ========================================
                else if (GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {
                    return INFINITY; // Disable Vehicle->GroundStation links in SatelliteNetworkConfigurator
                }
                // ========================================
                // Case 2.C: Vehicle -> Vehicle (not implemented because not needed)
                // V2V is managed by Veins itself, not by SatelliteNetworkConfigurator
                // This method is used for wireless links only between satellite and ground nodes (GS or Veins vehicles)
                // ========================================
                else if (veins::VeinsInetMobility *destVeins = dynamic_cast<veins::VeinsInetMobility*>(rxMob)) {
                    return INFINITY; // Disable V2V links in SatelliteNetworkConfigurator
                }
            }
            // ========================================
            // CASE 3: GroundStation -> X \in {Satellite, GroundStation}
            // ========================================
            else if (GroundStationMobility *sourceGS = dynamic_cast<GroundStationMobility*>(txMob))
            {
                // ========================================
                // Case 3.A: GroundStation -> Satellite
                // ========================================
                if(SatelliteMobility *destSat = dynamic_cast<SatelliteMobility*>(rxMob)) {
                    // Calculate distance GroundStation <-> Satellite (in km) using SatelliteMobility->getDistance
                    // Don't use GroundStation->getDistance calculates the 3D Euclidean distance between two points in ECEF coordinates, 
                    // but is not designed for orbital calculations.
                    double distKm = destSat->getDistance(sourceGS->getLUTPositionY(), sourceGS->getLUTPositionX(), 0.0); 
                    if(std::isnan(distKm) || distKm < 0) {
                        return INFINITY;
                    }
                    delay = calculatePropagationDelay(distKm);
                    return delay;
                }
                // ========================================
                // Case 3.B: GroundStation -> GroundStation
                // ========================================
                else if (GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {

                    // Calculate distance GroundStation <-> GroundStation (in km)
                    double distKm = sourceGS->getDistance(destGS->getLUTPositionY(), destGS->getLUTPositionX(), 0.0);
                    if(std::isnan(distKm) || distKm < 0) {
                        return INFINITY;
                    }
                    delay = calculatePropagationDelay(distKm);
                    return delay;
                }
            }
            // Default for non-satellite or unidentified links (value is set to INFINITY)
            return INFINITY;
        }
        else {
            // For metrics other than propagationDelay (e.g., dataRate, errorRate)
            return minLinkWeight; // or use return SatelliteNetworkConfigurator::computeWirelessLinkWeight(link, metric, parameters);
        }
    }

    double SatelliteNetworkConfiguratorScs::computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters) {
        
        //Exclude links with interfaces that are down or have no carrier
        if (isToExcludeLink(link)) {
            return INFINITY;
        }
        const char *costAttribute = parameters->getAttribute("cost");
        if (costAttribute != nullptr)
            return parseCostAttribute(costAttribute);
        Topology::Link *linkOut = static_cast<Topology::Link *>(link);
        if (!strcmp(metric, "hopCount"))
            return 1;
        else if (!strcmp(metric, "delay")) {
            cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLinkOutLocalGate()->findTransmissionChannel());
            if (transmissionChannel != nullptr)
                return transmissionChannel->getDelay().dbl();
            else
                return minLinkWeight;
        }
        else if (!strcmp(metric, "dataRate")) {
            cChannel *transmissionChannel = linkOut->getLinkOutLocalGate()->findTransmissionChannel();
            if (transmissionChannel != nullptr) {
                double dataRate = transmissionChannel->getNominalDatarate();
                return dataRate != 0 ? 1 / dataRate : minLinkWeight;
            }
            else
                return minLinkWeight;
        }
        else if (!strcmp(metric, "errorRate")) {
            cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLinkOutLocalGate()->findTransmissionChannel());
            if (transmissionChannel != nullptr) {
                inet::L3NetworkConfiguratorBase::InterfaceInfo *sourceInterfaceInfo = link->sourceInterfaceInfo;
                double bitErrorRate = transmissionChannel->getBitErrorRate();
                double packetErrorRate = 1.0 - pow(1.0 - bitErrorRate, sourceInterfaceInfo->networkInterface->getMtu());
                return minLinkWeight - log(1 - packetErrorRate);
            }
            else
                return minLinkWeight;
        }
        else if (!strcmp(metric, "propagationDelay")) {
                return minLinkWeight;
        }
        else
            throw cRuntimeError("Unknown metric");
    }

} // namespace inet
