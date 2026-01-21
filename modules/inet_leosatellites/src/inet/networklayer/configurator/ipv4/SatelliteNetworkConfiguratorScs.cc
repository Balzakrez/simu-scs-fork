//
// Copyright (C) 2004 OpenSim Ltd.
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
// 
// This program was developed based on INET Framework (https://inet.omnetpp.org/).
// The original code can be found at `inet/networklayer/configurator/ipv4/L3NetworkConfiguratorBase.cc` of INET-4.4.1.

#include "SatelliteNetworkConfiguratorScs.h"
#include "veins_inet_scs/VeinsInetMobility.h" // from scs_optional
#include "scs_utils/converter/PositionConverter.h"
#include "leosatellites/mobility/SatelliteMobility.h"
#include "leosatellites/mobility/GroundStationMobility.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"

Define_Module(inet::SatelliteNetworkConfiguratorScs);

namespace inet {


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
            // Cache PositionConverter to avoid repeated lookups
            posConverter = dynamic_cast<Satellite::PositionConverter*>(
                getSimulation()->getSystemModule()->getSubmodule("Pos")
            );
            
            if (!posConverter) {
                throw cRuntimeError("PositionConverter 'Pos' not found - Vehicle-to-Satellite links will not work");
            } else {
                EV_DETAIL << "PositionConverter successfully initialized" << endl;
            }
        }
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

    bool SatelliteNetworkConfiguratorScs::isProtectedRoute(Ipv4Route* route){
        if (route == nullptr || route->getInterface() == nullptr) { return false; }   
        std::string ifName = route->getInterface()->getInterfaceName();
        // Preserve Default Routes for Cellular Interfaces
        if (ifName == "cellular" || ifName.find("lte") != std::string::npos) { 
            if (route->getDestination().isUnspecified() 
                && route->getGateway().isUnspecified()
                && route->getNetmask().isUnspecified()) {
                    return true;
                }
        }
        return false;
    }

    void SatelliteNetworkConfiguratorScs::validateVeinsNodeIp(){
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            if (isVeinsNode(node->getModule())) {
                for (auto& entry : node->interfaceInfos) {
                    InterfaceInfo *info = static_cast<InterfaceInfo*>(entry);
                    if (info->networkInterface) {
                        auto *ipv4Data = info->networkInterface->getProtocolData<Ipv4InterfaceData>();
                        // Retrieve real IP address from interface protocol data if available
                        if (ipv4Data && !ipv4Data->getIPAddress().isUnspecified()) {
                            info->address = ipv4Data->getIPAddress().getInt();
                            info->netmask = ipv4Data->getNetmask().getInt();
                        }
                        else {
                            // Set default unspecified IP address and netmask
                            info->address = Ipv4Address::UNSPECIFIED_ADDRESS.getInt();
                            info->addressSpecifiedBits = Ipv4Address::ALLONES_ADDRESS.getInt();
                            info->netmask = Ipv4Address::UNSPECIFIED_ADDRESS.getInt();
                            info->netmaskSpecifiedBits = Ipv4Address::ALLONES_ADDRESS.getInt();
                        }
                    }
                    else{
                        throw cRuntimeError("Veins node interface has no Ipv4InterfaceData");
                    }
                }
            }
        }
    }

    bool SatelliteNetworkConfiguratorScs::isToExcludeLink(Link *link) {
        // Exclude links with interfaces that are down or have no carrier
        // isUp() = administrative state, hasCarrier() = physical connectivity
        // This ensures that disabled interfaces are excluded from routing
        if (link->sourceInterfaceInfo && link->sourceInterfaceInfo->networkInterface) {
            auto *srcIf = link->sourceInterfaceInfo->networkInterface;
            if (!srcIf->isUp() || !srcIf->hasCarrier()) {
                EV_DETAIL << "Wireless link excluded: source interface " 
                          << srcIf->getInterfaceName() 
                          << " is DOWN/no carrier (isUp=" << srcIf->isUp() 
                          << ", hasCarrier=" << srcIf->hasCarrier() << ")" << endl;
                return true;
            }
        }
        if (link->destinationInterfaceInfo && link->destinationInterfaceInfo->networkInterface) {
            auto *dstIf = link->destinationInterfaceInfo->networkInterface;
            if (!dstIf->isUp() || !dstIf->hasCarrier()) {
                EV_DETAIL << "Wireless link excluded: destination interface " 
                          << dstIf->getInterfaceName() 
                          << " is DOWN/no carrier (isUp=" << dstIf->isUp() 
                          << ", hasCarrier=" << dstIf->hasCarrier() << ")" << endl;
                return true;
            }
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
                bool isProtected = isProtectedRoute(route);
                std::cerr << "  Route[" << i << "]: " 
                        << " source=" << nodeName << " "
                        << " dest=" << route->getDestination() << "/netlen=" << route->getNetmask().getNetmaskLength()
                        << " netmask=" << route->getNetmask()
                        << " via " << route->getInterface()->getInterfaceName()
                        << " gw=" << route->getGateway()
                        << " PROTECTED=" << (isProtected ? "YES" : "NO")
                        << endl;
            }
        }
    }

    void SatelliteNetworkConfiguratorScs::reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement) {
        EV_DETAIL << "\nReinvoking SatelliteNetworkConfiguratorScs at " << simTime() << endl;

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

                if (isProtectedRoute(route) && isVeinsNode(node->getModule())) { continue; }
                
                routingTable->deleteRoute(route);
            }
            // 1.2 Clear multicast routing table
            for(int m = 0; m < node->routingTable->getNumMulticastRoutes(); m++){
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

        // 3. Validate IP addresses for dynamic nodes (SUMO/Veins)
        this->validateVeinsNodeIp();

        // 4. Recalculate routing
        SatelliteNetworkConfigurator::addStaticRoutes(topology, autorouteElement);
        SatelliteNetworkConfigurator::configureAllRoutingTables();

        // 5. Remove unreachable routes (links with INFINITY weight in computeWirelessLinkWeight)
        // this->removeWirelessUnreachableRoutes(autorouteElement); // not needed
        // This is to prevent satellites from adding default routes that may interfere with Veins nodes' routing
        // Already removed with addDefaultRoutes = default(false);

        // std::cerr << "AFTER REINVOKING ROUTING TABLE:" << std::endl;
        // printRoutingTable();

        // 6. Debug output
        if (par("dumpTopology").boolValue())
            dumpTopology(topology);

        if (par("dumpConfig").stringValue()[0])
            dumpConfiguration();
    }

    double SatelliteNetworkConfiguratorScs::computeWirelessLinkWeight(Link *link, const char *metric, cXMLElement *parameters) {
        EV_DETAIL << "Called computeWirelessLinkWeight between " << link->sourceInterfaceInfo->node->module->getFullName() << " and " << link->destinationInterfaceInfo->node->module->getFullName() << std::endl;

        const char *costAttribute = parameters->getAttribute("cost");
        if (costAttribute != nullptr)
            return parseCostAttribute(costAttribute);
        

        if (!strcmp(metric, "hopCount"))
            return 1;
            
        // Calculate propagation delay (used to determine if link exists)
        if (!strcmp(metric, "propagationDelay")) {

            if (!link->sourceInterfaceInfo || !link->destinationInterfaceInfo) {
                EV_WARN << "Link has null interface info, skipping" << endl;
                return INFINITY;
            }
            if (!link->sourceInterfaceInfo->node || !link->destinationInterfaceInfo->node) {
                EV_WARN << "Link interface has null node, skipping" << endl;
                return INFINITY;
            }
            // Exclude links with interfaces that are down or have no carrier
            if (isToExcludeLink(link)) {
                EV_WARN<< "Link excluded based on interface state" << endl;
                return INFINITY;
            }
            
            // Get transmitter and receiver modules
            cModule *transmitterModule = link->sourceInterfaceInfo->node->module;
            cModule *receiverModule = link->destinationInterfaceInfo->node->module;
            
            // Verify modules are valid and not deleted
            if (!transmitterModule || !receiverModule) {
                EV_WARN << "TX or RX module is null or deleted, skipping link" << endl;
                return INFINITY;
            }

            EV_DETAIL << "Evaluating link: " << transmitterModule->getFullName() << " -> " << receiverModule->getFullName() << endl;
            
            // Get mobility modules
            cModule* txMob = transmitterModule->getSubmodule("mobility");
            cModule* rxMob = receiverModule->getSubmodule("mobility");

            if(!txMob || !rxMob) {
                EV_WARN << "Mobility module missing in TX or RX" << endl;
                return INFINITY;
            }

            // Ensure PositionConverter is available
            if(!posConverter) {
                EV_WARN << "PositionConverter not available for Veins nodes" << endl;
                return INFINITY;
            }

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
                        EV_WARN << "SAT->VEC: Dest vehicle module deleted/invalid" << endl;
                        return INFINITY;
                    }

                    // Get vehicle Cartesian position
                    inet::Coord pos = destVeins->getCurrentPosition();

                    // Validate position
                    if(!isValidVeinsPosition(pos)) {
                        EV_WARN << "SAT->VEC: " << sourceSat->getFullPath() <<  " -> " << destVeins->getFullPath() << ": Invalid position" << endl;
                        return INFINITY;
                    }

                    // Convert X,Y -> Lat,Lon using PositionConverter
                    double vehLon = posConverter->convertPosXToLongitude(pos.x);
                    double vehLat = posConverter->convertPosYToLatitude(pos.y);

                    EV_DETAIL << "VEC pos: (" << pos.x << "m, " << pos.y << "m) -> (" << vehLat << "°, " 
                            << vehLon << "°), SAT: (" << sourceSat->getLatitude() << "°, " 
                            << sourceSat->getLongitude() << "°, " << sourceSat->getAltitude() 
                            << "km), Elev: " << sourceSat->getElevation(vehLat, vehLon, 0.0) << "°" << endl;

                    if (!sourceSat->isReachable(vehLat, vehLon, 0.0)) {   
                        EV_WARN << "SAT->VEC:" << sourceSat->getFullPath() << " -> " << destVeins->getFullPath() << ": Link not reachable" << endl;
                        return INFINITY;
                    }

                    // Calculate distance Satellite <-> Vehicle (in km)
                    double distKm = sourceSat->getDistance(vehLat, vehLon, 0.0);

                    if(std::isnan(distKm) || distKm < 0) { 
                        EV_WARN << "SAT->VEC:" << sourceSat->getFullPath() << " -> " << destVeins->getFullPath() << ": Invalid distance " << distKm << " km" << endl;
                        return INFINITY;
                    }
                    
                    // Calculate propagation delay (distance / speed of light)
                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "SAT->VEC:" 
                              << sourceSat->getFullPath() << " -> " << destVeins->getFullPath() 
                              << " | Dist: " << distKm  << "km, Delay: " << delay << "s" << endl;
                    
                    return delay; 
                }
                // ========================================
                // CASE 1.B: Satellite -> GroundStation
                // ========================================
                else if(GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {

                    // Calculate distance Satellite <-> GroundStation (in km)
                    double distKm = sourceSat->getDistance(destGS->getLUTPositionY(), destGS->getLUTPositionX(), 0.0);

                    if(std::isnan(distKm) || distKm < 0) {
                        EV_WARN << "SAT->GS:" << sourceSat->getFullPath() << " -> " << destGS->getFullPath() << ": Invalid distance " << distKm << " km" << endl;
                        return INFINITY;
                    }

                    // Calculate propagation delay (distance / speed of light)
                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "SAT->GS:" 
                            << sourceSat->getFullPath() << " -> " << destGS->getFullPath() 
                            << " | Dist: " << distKm << "km, Delay: " << delay << "s" << endl;

                    return delay;
                }
                // ========================================
                // CASE 1.C: Satellite -> Satellite 
                // ========================================
                else if(SatelliteMobility *destSat = dynamic_cast<SatelliteMobility*>(rxMob)) {

                    // Calculate distance Satellite <-> Satellite (in km)
                    double distKm = sourceSat->getDistance(destSat->getLatitude(), destSat->getLongitude(), destSat->getAltitude());

   
                    if(std::isnan(distKm) || distKm < 0) {  
                        EV_WARN << "SAT->SAT:" << sourceSat->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid distance " << distKm << " km" << endl;
                        return INFINITY;
                    }

                    // Calculate propagation delay (distance / speed of light)
                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "SAT->SAT:" 
                              << sourceSat->getFullPath() << " -> " << destSat->getFullPath() 
                              << " | Dist: " << distKm << "km, Delay: " << delay << "s" << endl;
                    
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
                    EV_WARN << "Source vehicle module deleted/invalid" << endl;
                    return INFINITY;
                }
                // ========================================
                // Case 2.A: Vehicle -> Satellite
                // ========================================
                if(SatelliteMobility *destSat = dynamic_cast<SatelliteMobility*>(rxMob)) {
                    // Get vehicle Cartesian position
                    inet::Coord pos = sourceVeins->getCurrentPosition();
           
                    // Validate position
                    if(!isValidVeinsPosition(pos)) {
                        EV_DETAIL << "VEC->SAT:" << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid position" << endl;
                        return INFINITY;
                    }

                    // Convert X,Y -> Lat,Lon using PositionConverter
                    double vehLon = posConverter->convertPosXToLongitude(pos.x);
                    double vehLat = posConverter->convertPosYToLatitude(pos.y);

                    if (!destSat->isReachable(vehLat, vehLon, 0.0)) {   
                        EV_DETAIL << "VEC->SAT:" << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() << ": Link not reachable" << endl;
                        return INFINITY;
                    }

                    EV_DETAIL << "VEC pos: (" << pos.x << "m, " << pos.y << "m) -> (" << vehLat << "°, " 
                             << vehLon << "°), SAT: (" << destSat->getLatitude() << "°, " 
                             << destSat->getLongitude() << "°, " << destSat->getAltitude() 
                             << "km), Elev: " << destSat->getElevation(vehLat, vehLon, 0.0) << "°" << endl;

                    // Calculate distance Satellite <-> Vehicle (in km)
                    double distKm = destSat->getDistance(vehLat, vehLon, 0.0);

                    if(std::isnan(distKm) || distKm < 0) { 
                        EV_WARN << "VEC->SAT:" << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid distance " << distKm << " km" << endl;
                        return INFINITY;
                    }

                    // Calculate propagation delay (distance / speed of light)
                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "VEC->SAT:" 
                            << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() 
                            << " | Dist: " << distKm << "km, Delay: " << delay << "s" << endl;
                    
                    return delay;
                }
                // ========================================
                // Case 2.B: Vehicle -> GroundStation (not implemented because not needed)
                // Use Vehicle->Satellite->GroundStation links instead
                // This method is used for wireless links only between satellite and ground nodes (GS or Veins vehicles)
                // ========================================
                else if (GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {
                    EV_DETAIL << "VEC -> GS: " << sourceVeins->getFullPath() << " -> " << destGS->getFullPath() << endl;
                    return INFINITY; // Disable Vehicle->GroundStation links in SatelliteNetworkConfigurator
                }
                // ========================================
                // Case 2.C: Vehicle -> Vehicle (not implemented because not needed)
                // V2V is managed by Veins itself, not by SatelliteNetworkConfigurator
                // This method is used for wireless links only between satellite and ground nodes (GS or Veins vehicles)
                // ========================================
                else if (veins::VeinsInetMobility *destVeins = dynamic_cast<veins::VeinsInetMobility*>(rxMob)) {
                    EV_DETAIL << "VEC -> VEC: " << sourceVeins->getFullPath() << " -> " << destVeins->getFullPath() << endl;
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
                        EV_WARN << "GS->SAT:" << sourceGS->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid distance "<< distKm << " km" << endl;
                        return INFINITY;
                    }

                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "GS->SAT:" << sourceGS->getFullPath() << " -> " << destSat->getFullPath() 
                        << " | Dist: " << distKm << "km, Delay: " << delay << "s" << endl;

                    return delay;
                }
                // ========================================
                // Case 3.B: GroundStation -> GroundStation
                // ========================================
                else if (GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {

                    // Calculate distance GroundStation <-> GroundStation (in km)
                    double distKm = sourceGS->getDistance(destGS->getLUTPositionY(), destGS->getLUTPositionX(), 0.0);

                    if(std::isnan(distKm) || distKm < 0) {
                        EV_WARN << "GS->GS:" << sourceGS->getFullPath() << " -> " << destGS->getFullPath() << ": Invalid distance " << distKm << " km" << endl;
                        return INFINITY;
                    }

                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "GS->GS:" << sourceGS->getFullPath() << " -> " << destGS->getFullPath() 
                        << " | Dist: " << distKm << "km, Delay: " << delay << "s" << endl;

                    return delay;
                }
            }
            // Default for non-satellite or unidentified links (value is set to INFINITY)
            EV_WARN << "Unhandled mobility case: "
                << "TX=" << transmitterModule->getFullPath() << " (" << (txMob ? txMob->getClassName() : "NULL") << "), " 
                << "RX=" << receiverModule->getFullPath() << " (" << (rxMob ? rxMob->getClassName() : "NULL") << ")" << endl;
            return INFINITY;
        }
        else {
            // For metrics other than propagationDelay (e.g., dataRate, errorRate)
            EV_WARN << "Metric '" << metric << "' not implemented for wireless links, using minLinkWeight" << endl;
            return minLinkWeight; // or use return SatelliteNetworkConfigurator::computeWirelessLinkWeight(link, metric, parameters);
        }
    }

    double SatelliteNetworkConfiguratorScs::computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters) {
        const char *costAttribute = parameters->getAttribute("cost");
        if (costAttribute != nullptr)
            return parseCostAttribute(costAttribute);
        
        Topology::Link *linkOut = static_cast<Topology::Link *>(static_cast<Topology::Link *>(link));
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
                EV_DETAIL << "Computing wired link weight (propagationDelay) for link from" 
                  << (link->sourceInterfaceInfo && link->sourceInterfaceInfo->node && link->sourceInterfaceInfo->node->module ? link->sourceInterfaceInfo->node->module->getFullName() : "NULL") 
                  << " to " 
                  << (link->destinationInterfaceInfo && link->destinationInterfaceInfo->node && link->destinationInterfaceInfo->node->module ? link->destinationInterfaceInfo->node->module->getFullName() : "NULL") 
                  << endl;
                return minLinkWeight;
        }
        else
            throw cRuntimeError("Unknown metric");
    }


} // namespace inet
