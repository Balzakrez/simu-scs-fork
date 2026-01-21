// 
// Copyright (C) 2024 Giusepppe Balzano
//
// HybridInterfaceManager.cc - Implementation of hard switching between satellite and cellular interfaces
//

#include "HybridInterfaceManager.h"

Define_Module(HybridInterfaceManager);

HybridInterfaceManager::~HybridInterfaceManager() {
    if (this->strategy) {
        delete this->strategy;
        this->strategy = nullptr;
    }
}

void HybridInterfaceManager::initialize(int stage) {
    // INITSTAGE_LOCAL: Load basic parameters and register signals
    if (stage == INITSTAGE_LOCAL) {
        // Load configuration parameters
        this->satInterfaceName = par("satInterfaceName").stringValue();
        this->cellInterfaceName = par("cellInterfaceName").stringValue();
        this->isSatState = par("startWithSatellite").boolValue();
        this->switchingMode = par("switchingMode").stringValue();

        // Register Switching signals
        this->interfaceSignalId = registerSignal("interfaceSignal");
        this->switchSignalId = registerSignal("switchSignal");

        // Register Interface metrics signals
        this->satUsageTimeSignalId = registerSignal("satUsageSignal");
        this->cellUsageTimeSignalId = registerSignal("cellUsageSignal");

        // Initialize counters and timers
        this->totalSwitchesCount = 0;
        this->lastSwitchTime = simTime(); // Initialize to current simulation time
        this->satTotalTime = SimTime::ZERO;
        this->cellTotalTime = SimTime::ZERO;
    }
    // INITSTAGE_NETWORK_CONFIGURATION: Interfaces are already registered and configured
    if (stage == INITSTAGE_NETWORK_CONFIGURATION) {
        // Get InterfaceTable and RoutingTable
        cModule *host = getParentModule();
        // std::string currNodeName = getParentModule()->getFullName();
        interfaceTable = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        routingTable = dynamic_cast<IIpv4RoutingTable*>(host->getSubmodule("ipv4")->getSubmodule("routingTable"));
        // Find the specific interfaces
        satInterface = interfaceTable->findInterfaceByName(satInterfaceName.c_str());
        cellInterface = interfaceTable->findInterfaceByName(cellInterfaceName.c_str());
        
        // Apply initial states based on satState parameter: true=sat active, false=cellular active
        updateInterfaceStates();
    }
    // INITSTAGE_APPLICATION_LAYER: Create and initialize strategy
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        strategy = createStrategy();
        strategy->initialize(stage);
        EV_INFO << "Strategy initialized: " << strategy->getStrategyName() << std::endl;
    }
}

ISwitchingStrategy* HybridInterfaceManager::createStrategy() {
    if (switchingMode == "time-based") {
        return new TimeBasedStrategy(this);
    } 
    else if (switchingMode == "coverage-based") {
        return new CoverageBasedStrategy(this);
    }
    else if (switchingMode == "energy-based") {
        return new EnergyBasedStrategy(this);  
    }
    else {
        throw cRuntimeError("Unknown switching mode: %s", switchingMode.c_str());
    }
}

void HybridInterfaceManager::handleMessage(cMessage *msg) {
    // Delegate to strategy if applicable
    if (strategy) {
        strategy->handleMessage(msg);
    } else {
        EV_WARN << "Unexpected message received, ignored." << std::endl;
        delete msg;
    }
}

void HybridInterfaceManager::performSwitch(bool toSatellite) {
    if (this->isSatState == toSatellite) { // Current state matches requested state; no action needed
        return; 
    } else {
        simtime_t timeSinceLastSwitch = simTime() - this->lastSwitchTime;
        if (toSatellite) 
            this->cellTotalTime += timeSinceLastSwitch; // Update cellular usage time before switch to satellite
        else 
            this->satTotalTime += timeSinceLastSwitch; // Update satellite usage time before switch to cellular
    }
    
    // Update Satellite State with requested state
    this->isSatState = toSatellite;

    // Update time trackers and counters
    this->totalSwitchesCount++; // Increment switch counter
    this->lastSwitchTime = simTime(); // Update last switch timestamp

    // Emit statistics
    emit(this->interfaceSignalId, this->isSatState ? 1 : 0);
    emit(this->switchSignalId, this->totalSwitchesCount);
    emit(this->satUsageTimeSignalId, SIMTIME_DBL(this->satTotalTime));
    emit(this->cellUsageTimeSignalId, SIMTIME_DBL(this->cellTotalTime));

    updateInterfaceStates(); // Apply the new interface states
}


void HybridInterfaceManager::updateInterfaceStates() {
    if (satInterface != nullptr) {
        NetworkInterface::State targetState = isSatState ? NetworkInterface::UP : NetworkInterface::DOWN;
        // ADMINISTRATIVE STATE (UP/DOWN): Check if interface is utilizable for routing or not
        if (satInterface->getState() != targetState) {
            satInterface->setState(targetState);
            EV_DETAIL << "SatInterface State updated to (State=" << (isSatState ? "UP" : "DOWN") 
                      << ", Carrier=" << satInterface->hasCarrier() << ")" << std::endl;
        }
        // CARRIER STATE (hasCarrier): Check if interface can physically send/receive data
        if (satInterface->hasCarrier() != isSatState) {
            satInterface->setCarrier(isSatState);
            EV_DETAIL << "SatInterface Carrier updated to (State=" << (isSatState ? "UP" : "DOWN") 
                      << ", Carrier=" << satInterface->hasCarrier() << ")" << std::endl;
        }
        // if(isSatState){
        //     ensureSatelliteDefaultRoute(); // not needed
        // }
    }
    if (cellInterface != nullptr) {
        bool cellularState = !isSatState;
        // ADMINISTRATIVE STATE (UP/DOWN): Check if interface is utilizable for routing or not
        NetworkInterface::State targetState = cellularState ? NetworkInterface::UP : NetworkInterface::DOWN;
        if (cellInterface->getState() != targetState) {
            cellInterface->setState(targetState);
            EV_DETAIL << "CellInterface State updated to (State=" << (cellularState ? "UP" : "DOWN") 
                      << ", Carrier=" << cellInterface->hasCarrier() << ")" << std::endl;
        }
        // CARRIER STATE (hasCarrier): Check if interface can physically send/receive data
        if (cellInterface->hasCarrier() != cellularState){
            cellInterface->setCarrier(cellularState);
              EV_DETAIL << "CellInterface Carrier updated to (State=" << (cellularState ? "UP" : "DOWN") 
                      << ", Carrier=" << cellInterface->hasCarrier() << ")" << std::endl;
        }
        if (cellularState){
            ensureCellularDefaultRoute();
        }
    }
}


void HybridInterfaceManager::ensureCellularDefaultRoute() {
    if (!routingTable || !cellInterface) {
        EV_WARN << "Cannot manage cellular route (routingTable or cellInterface null)" << std::endl;
        return;
    }
    Ipv4Route *defaultRoute = new Ipv4Route();
    defaultRoute->setDestination(Ipv4Address::UNSPECIFIED_ADDRESS);
    defaultRoute->setNetmask(Ipv4Address::UNSPECIFIED_ADDRESS);
    defaultRoute->setGateway(Ipv4Address::UNSPECIFIED_ADDRESS);
    defaultRoute->setInterface(cellInterface);
    defaultRoute->setSourceType(IRoute::MANUAL);
    // defaultRoute->setMetric(20); // Set high value for low priority (default route)
    routingTable->addRoute(defaultRoute);
    EV_DETAIL << "Default CELLULAR route added" << std::endl;
} 
   

// void HybridInterfaceManager::ensureSatelliteDefaultRoute() {
//     if (!routingTable || !satInterface){
//         EV_WARN << "Cannot manage satellite route (routingTable or satInterface null)" << std::endl;
//         return;
//     }
//     Ipv4Route *defaultRoute = new Ipv4Route();
//     defaultRoute->setDestination(Ipv4Address::UNSPECIFIED_ADDRESS);
//     defaultRoute->setNetmask(Ipv4Address::UNSPECIFIED_ADDRESS);
//     defaultRoute->setGateway(Ipv4Address("10.1.0.1"));
//     defaultRoute->setInterface(satInterface);
//     defaultRoute->setSourceType(IRoute::MANUAL);
//     // defaultRoute->setMetric(20); // Set high value for low priority (default route)
//     routingTable->addRoute(defaultRoute);
//     EV_DETAIL << "Default SATELLITE route added with GW:" << defaultRoute->getGateway() << std::endl;
// }


