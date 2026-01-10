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
                EV_WARN << "PositionConverter 'Pos' not found - Vehicle-to-Satellite links will not work" << std::endl;
            } else {
                EV_INFO << "PositionConverter successfully initialized" << std::endl;
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


    void SatelliteNetworkConfiguratorScs::reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement) {
        EV_INFO << "reinvokeConfigurator @ t=" << simTime() << endl;
        
        // Lambda to check if a node is a Veins/Hybrid vehicle node
        auto isVeinsNode = [](cModule *mod) {
            if (mod == nullptr) return false;
            if (mod->getSubmodule("mobility") == nullptr) return false;
            return (dynamic_cast<veins::VeinsInetMobility*>(mod->getSubmodule("mobility"))) != nullptr;
        };
        
        // Lambda to check if a route is a cellular DEFAULT route (not specific routes)
        // We only preserve the default route (0.0.0.0/0) via cellular
        // Specific routes will be recalculated by the configurator
        auto isCellularDefaultRoute = [](Ipv4Route *route) {
            if (route == nullptr || route->getInterface() == nullptr) return false;
            // Must be a default route (destination and netmask unspecified)
            if (!route->getDestination().isUnspecified() || !route->getNetmask().isUnspecified()) {
                return false;
            }
            std::string ifName = route->getInterface()->getInterfaceName();
            return (ifName == "cellular" || ifName.find("lte") != std::string::npos);
        };
        
        // 1. Clean routing tables and topology
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            node->interfaceInfos.clear();
            if (node->getModule() == nullptr) continue;
            Ipv4RoutingTable *routingTable = dynamic_cast<Ipv4RoutingTable*>(node->routingTable);
            
            // Per i veicoli, preserva SOLO la default route cellular
            bool isVehicle = isVeinsNode(node->getModule());
            
            // DEBUG: log route count before clearing
            if (isVehicle) {
                EV_DETAIL << "Clearing routes for " << node->getModule()->getFullName() 
                          << " (currently has " << routingTable->getNumRoutes() << " routes)" << endl;
            }
            
            // Clear routing table (ma preserva la default route cellular per veicoli)
            int deleted = 0;
            for(int j = routingTable->getNumRoutes() - 1; j >= 0; j--){
                Ipv4Route *route = routingTable->getRoute(j);
                if (isVehicle && isCellularDefaultRoute(route)) {
                    EV_DETAIL << "Preserving cellular DEFAULT route for " 
                              << node->getModule()->getFullName() << endl;
                    continue; // Do not delete this route
                }
                routingTable->deleteRoute(route);
                deleted++;
            }
            
            if (isVehicle && deleted > 0) {
                EV_DETAIL << "Deleted " << deleted << " routes from " 
                          << node->getModule()->getFullName() << ", remaining: " 
                          << routingTable->getNumRoutes() << endl;
            }
            // Clear multicast routing table
            for(int m = 0; m < node->routingTable->getNumMulticastRoutes(); m++){
                node->routingTable->deleteMulticastRoute(node->routingTable->getMulticastRoute(m));
            }
            // Clear static routes
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

        // 3. Handle IP addresses for dynamic nodes (SUMO/Veins)
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            if (isVeinsNode(node->getModule())) {
                for (auto& entry : node->interfaceInfos) {
                    InterfaceInfo *info = static_cast<InterfaceInfo*>(entry);
                    // Set default unspecified IP address and netmask
                    info->address = Ipv4Address::UNSPECIFIED_ADDRESS.getInt();
                    info->addressSpecifiedBits = Ipv4Address::ALLONES_ADDRESS.getInt();
                    info->netmask = Ipv4Address::UNSPECIFIED_ADDRESS.getInt();
                    info->netmaskSpecifiedBits = Ipv4Address::ALLONES_ADDRESS.getInt();
                    // Retrieve real IP address from interface protocol data if available
                    if (info && info->networkInterface) {
                        auto *ipv4Data = info->networkInterface->getProtocolData<Ipv4InterfaceData>();
                        if (ipv4Data && !ipv4Data->getIPAddress().isUnspecified()) {
                            info->address = ipv4Data->getIPAddress().getInt();
                            info->netmask = ipv4Data->getNetmask().getInt();
                        }
                    }
                }
            }
        }
        
        // 4. Recalculate routing
        SatelliteNetworkConfigurator::addStaticRoutes(topology, autorouteElement);
        SatelliteNetworkConfigurator::configureAllRoutingTables();
        
        // 5. Post-processing: remove routes via interfaces without carrier (for vehicles)
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            if (isVeinsNode(node->getModule())) {
                Ipv4RoutingTable *routingTable = dynamic_cast<Ipv4RoutingTable*>(node->routingTable);
                if (routingTable) {
                    int removed = 0;
                    for (int r = routingTable->getNumRoutes() - 1; r >= 0; r--) {
                        Ipv4Route *route = routingTable->getRoute(r);
                        NetworkInterface *routeIf = route->getInterface();
                        if (routeIf && !routeIf->hasCarrier()) {
                            routingTable->deleteRoute(route);
                            removed++;
                        }
                    }
                    if (removed > 0) {
                        EV_DETAIL << "Post-processing: removed " << removed 
                                  << " routes via no-carrier interfaces for " << node->getModule()->getFullName() 
                                  << " (remaining: " << routingTable->getNumRoutes() << ")" << endl;
                    }
                }
            }
        }

        // 6. Debug output
        if (par("dumpTopology").boolValue())
            dumpTopology(topology);

        if (par("dumpConfig").stringValue()[0])
            dumpConfiguration();
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
    

    double SatelliteNetworkConfiguratorScs::computeWirelessLinkWeight(Link *link, const char *metric, cXMLElement *parameters) {
        const char *costAttribute = parameters->getAttribute("cost");
        if (costAttribute != nullptr)
            return parseCostAttribute(costAttribute);

        // ========================================
        // CHECK INTERFACE STATE: If either interface is DOWN or has no carrier, return INFINITY
        // isUp() = administrative state, hasCarrier() = physical connectivity
        // This ensures that disabled interfaces are excluded from routing
        // ========================================
        if (link->sourceInterfaceInfo && link->sourceInterfaceInfo->networkInterface) {
            auto *srcIf = link->sourceInterfaceInfo->networkInterface;
            if (!srcIf->isUp() || !srcIf->hasCarrier()) {
                EV_DETAIL << "Wireless link excluded: source interface " 
                          << srcIf->getInterfaceName() 
                          << " is DOWN/no carrier (isUp=" << srcIf->isUp() 
                          << ", hasCarrier=" << srcIf->hasCarrier() << ")" << endl;
                return INFINITY;
            }
        }
        if (link->destinationInterfaceInfo && link->destinationInterfaceInfo->networkInterface) {
            auto *dstIf = link->destinationInterfaceInfo->networkInterface;
            if (!dstIf->isUp() || !dstIf->hasCarrier()) {
                EV_DETAIL << "Wireless link excluded: destination interface " 
                          << dstIf->getInterfaceName() 
                          << " is DOWN/no carrier (isUp=" << dstIf->isUp() 
                          << ", hasCarrier=" << dstIf->hasCarrier() << ")" << endl;
                return INFINITY;
            }
        }
        
        if (!strcmp(metric, "hopCount"))
            return 1;
            
        // Calculate propagation delay (used to determine if link exists)
        if (!strcmp(metric, "propagationDelay")) {
            
            // ========================================
            // NULL POINTER SAFETY CHECKS
            // Verify all required pointers are valid before accessing them
            // ========================================
            if (!link->sourceInterfaceInfo || !link->destinationInterfaceInfo) {
                EV_DETAIL << "Link has null interface info, skipping" << endl;
                return INFINITY;
            }
            if (!link->sourceInterfaceInfo->node || !link->destinationInterfaceInfo->node) {
                EV_DETAIL << "Link interface has null node, skipping" << endl;
                return INFINITY;
            }
            
            // Get transmitter and receiver modules
            cModule *transmitterModule = link->sourceInterfaceInfo->node->module;
            cModule *receiverModule = link->destinationInterfaceInfo->node->module;
            
            // Verify modules are valid and not deleted
            if (!transmitterModule || !receiverModule) {
                EV_DETAIL << "TX or RX module is null or deleted, skipping link" << endl;
                return INFINITY;
            }
            
            // Get mobility modules
            cModule* txMob = transmitterModule->getSubmodule("mobility");
            cModule* rxMob = receiverModule->getSubmodule("mobility");

            if(!txMob || !rxMob) {
                EV_DETAIL << "Mobility module missing in TX or RX" << std::endl;
                return INFINITY;
            }
            
            // Ensure PositionConverter is available
            if(!posConverter) {
                EV_WARN << "PositionConverter not available for Veins nodes" << std::endl;
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
                        EV_WARN << "Dest vehicle module deleted/invalid" << endl;
                        return INFINITY;
                    }

                    // Get vehicle Cartesian position
                    inet::Coord pos = destVeins->getCurrentPosition();

                    // Validate position
                    if(!isValidVeinsPosition(pos)) {
                        EV_WARN << "SAT->VEC: " << sourceSat->getFullPath() <<  " -> " << destVeins->getFullPath() << ": Invalid position" << std::endl;
                        return INFINITY;
                    }

                    // Convert X,Y -> Lat,Lon using PositionConverter
                    double vehLon = posConverter->convertPosXToLongitude(pos.x);
                    double vehLat = posConverter->convertPosYToLatitude(pos.y);

                    EV_TRACE << "VEC pos: (" << pos.x << "m, " << pos.y << "m) -> (" << vehLat << "°, " 
                             << vehLon << "°), SAT: (" << sourceSat->getLatitude() << "°, " 
                             << sourceSat->getLongitude() << "°, " << sourceSat->getAltitude() 
                             << "km), Elev: " << sourceSat->getElevation(vehLat, vehLon, 0.0) << "°" << std::endl;

                    if (!sourceSat->isReachable(vehLat, vehLon, 0.0)) {   
                        EV_DETAIL << "SAT->VEC:" << sourceSat->getFullPath() << " -> " << destVeins->getFullPath() << ": Link not reachable" << std::endl;
                        return INFINITY;
                    }

                    // Calculate distance Satellite <-> Vehicle (in km)
                    double distKm = sourceSat->getDistance(vehLat, vehLon, 0.0);

                    // Validate distance: LEO satellites typically have a max range of ~2000 km from vehicles
                    if(std::isnan(distKm) || distKm < 0 || distKm > 2000.0) { 
                        EV_WARN << "SAT->VEC:" << sourceSat->getFullPath() << " -> " << destVeins->getFullPath() << ": Invalid distance " << distKm << " km (max 2000km for LEO)" << std::endl;
                        return INFINITY;
                    }
                    
                    // Calculate propagation delay (distance / speed of light)
                    delay = (distKm * 1000.0) / 299792458.0;
                    
                    EV_DETAIL << "SAT->VEC:" << sourceSat->getFullPath() << " -> " << destVeins->getFullPath() 
                                << " | Dist: " << distKm  << "km, Delay: " << delay << "s @ t=" << simTime() << std::endl;
                    
                    
                    return delay; 
                }
                // ========================================
                // CASE 1.B: Satellite -> GroundStation
                // ========================================
                else if(GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {

                    // Calculate distance Satellite <-> GroundStation (in km)
                    double distKm = sourceSat->getDistance(destGS->getLUTPositionY(), destGS->getLUTPositionX(), 0.0);

                    // Validate distance: LEO satellites typically have a max range of ~2500 km from GS
                    if(std::isnan(distKm) || distKm < 0 || distKm > 2500.0) {
                        EV_WARN << "SAT->GS:" << sourceSat->getFullPath() << " -> " << destGS->getFullPath() << ": Invalid distance " << distKm << " km" << std::endl;
                        return INFINITY;
                    }

                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "SAT->GS:" << sourceSat->getFullPath() << " -> " << destGS->getFullPath() 
                        << " | Dist: " << distKm << "km, Delay: " << delay << "s" << std::endl;

                    return delay;
                }
                // ========================================
                // CASE 1.C: Satellite -> Satellite 
                // ========================================
                else if(SatelliteMobility *destSat = dynamic_cast<SatelliteMobility*>(rxMob)) {

                    // Calculate distance Satellite <-> Satellite (in km)
                    double distKm = sourceSat->getDistance(destSat->getLatitude(), destSat->getLongitude(), destSat->getAltitude());

                    // Validate distance: Inter-Satellite Links typically have a max range of ~5000 km from SAT
                    if(std::isnan(distKm) || distKm < 0 || distKm > 5000.0) {  
                        EV_WARN << "SAT->SAT:" << sourceSat->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid distance " << distKm << " km (max 5000km)" << std::endl;
                        return INFINITY;
                    }

                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "SAT->SAT:" << sourceSat->getFullPath() << " -> " << destSat->getFullPath() 
                        << " | Dist: " << distKm << "km, Delay: " << delay << "s" << std::endl;
                    
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
                        EV_WARN << "VEC->SAT:" << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid position" << std::endl;
                        return INFINITY;
                    }

                    // Convert X,Y -> Lat,Lon using PositionConverter
                    double vehLon = posConverter->convertPosXToLongitude(pos.x);
                    double vehLat = posConverter->convertPosYToLatitude(pos.y);
               
                    if (!destSat->isReachable(vehLat, vehLon, 0.0)) {   
                        EV_DETAIL << "VEC->SAT:" << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() << ": Link not reachable" << std::endl;
                        return INFINITY;
                    }

                    // Calculate distance Satellite <-> Vehicle (in km)
                    double distKm = destSat->getDistance(vehLat, vehLon, 0.0);

                    // Validate distance: LEO satellites typically have a max range of ~2000 km from vehicles
                    if(std::isnan(distKm) || distKm < 0 || distKm > 2000.0) { 
                        EV_WARN << "VEC->SAT:" << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid distance " << distKm << " km (max 2000km for LEO)" << std::endl;
                        return INFINITY;
                    }

                    // Calculate propagation delay (distance / speed of light)
                    delay = (distKm * 1000.0) / 299792458.0;

                    EV_DETAIL << "VEC->SAT:" << sourceVeins->getFullPath() << " -> " << destSat->getFullPath() 
                        << " | Dist: " << distKm << "km, Delay: " << delay << "s @ t=" << simTime() << std::endl;
                    
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

                    // Validate distance: LEO satellites typically have a max range of ~2500 km from GS
                    if(std::isnan(distKm) || distKm < 0 || distKm > 2500.0) {
                        EV_WARN << "GS->SAT:" << sourceGS->getFullPath() << " -> " << destSat->getFullPath() << ": Invalid distance "<< distKm << " km (max 2500km for LEO)" << std::endl;
                        return INFINITY;
                    }

                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "GS->SAT:" << sourceGS->getFullPath() << " -> " << destSat->getFullPath() 
                        << " | Dist: " << distKm << "km, Delay: " << delay << "s" << std::endl;

                    return delay;
                }
                // ========================================
                // Case 3.B: GroundStation -> GroundStation
                // ========================================
                else if (GroundStationMobility *destGS = dynamic_cast<GroundStationMobility*>(rxMob)) {

                    // Calculate distance GroundStation <-> GroundStation (in km)
                    double distKm = sourceGS->getDistance(destGS->getLUTPositionY(), destGS->getLUTPositionX(), 0.0);

                    if(std::isnan(distKm) || distKm < 0) {
                        EV_WARN << "GS->GS:" << sourceGS->getFullPath() << " -> " << destGS->getFullPath() << ": Invalid distance " << distKm << " km" << std::endl;
                        return INFINITY;
                    }

                    delay = (distKm * 1000.0) / 299792458.0;
                    EV_DETAIL << "GS->GS:" << sourceGS->getFullPath() << " -> " << destGS->getFullPath() 
                        << " | Dist: " << distKm << "km, Delay: " << delay << "s" << std::endl;

                    return delay;
                }
            }
            // Default for non-satellite or unidentified links (value is set to INFINITY)
            EV_WARN << "Unhandled mobility case: TX=" << transmitterModule->getFullPath() 
                    << " (" << (txMob ? txMob->getClassName() : "NULL") << "), RX=" 
                    << receiverModule->getFullPath() << " (" << (rxMob ? rxMob->getClassName() : "NULL") 
                    << ")" << std::endl;
            return INFINITY;
        }
        else {
            // For metrics other than propagationDelay (e.g., dataRate, errorRate)
            EV_WARN << "Metric '" << metric << "' not implemented for wireless links, using minLinkWeight" << std::endl;
            return minLinkWeight; // or use return SatelliteNetworkConfigurator::computeWirelessLinkWeight(link, metric, parameters);
        }
    }

   
    double SatelliteNetworkConfiguratorScs::computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters) {
        const char *costAttribute = parameters->getAttribute("cost");
        if (costAttribute != nullptr)
            return parseCostAttribute(costAttribute);
        
        // ========================================
        // CHECK INTERFACE STATE: If either interface is DOWN or has no carrier, return INFINITY
        // isUp() = administrative state, hasCarrier() = physical connectivity
        // This ensures that disabled interfaces are excluded from routing
        // ========================================
        if (link->sourceInterfaceInfo && link->sourceInterfaceInfo->networkInterface) {
            auto *srcIf = link->sourceInterfaceInfo->networkInterface;
            if (!srcIf->isUp() || !srcIf->hasCarrier()) {
                EV_DETAIL << "Wired link excluded: source interface " 
                          << srcIf->getInterfaceName() 
                          << " is DOWN/no carrier (isUp=" << srcIf->isUp() 
                          << ", hasCarrier=" << srcIf->hasCarrier() << ")" << endl;
                return INFINITY;
            }
        }
        if (link->destinationInterfaceInfo && link->destinationInterfaceInfo->networkInterface) {
            auto *dstIf = link->destinationInterfaceInfo->networkInterface;
            if (!dstIf->isUp() || !dstIf->hasCarrier()) {
                EV_DETAIL << "Wired link excluded: destination interface " 
                          << dstIf->getInterfaceName() 
                          << " is DOWN/no carrier (isUp=" << dstIf->isUp() 
                          << ", hasCarrier=" << dstIf->hasCarrier() << ")" << endl;
                return INFINITY;
            }
        }
        
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
                return minLinkWeight;
        }
        else
            throw cRuntimeError("Unknown metric");
    }
} // namespace inet
