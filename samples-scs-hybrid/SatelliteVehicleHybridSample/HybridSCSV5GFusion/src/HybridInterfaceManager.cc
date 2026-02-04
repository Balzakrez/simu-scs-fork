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
    if (switchGuardTimerMsg) {
        cancelAndDelete(switchGuardTimerMsg);
        switchGuardTimerMsg = nullptr;
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
        this->lastInterfaceActiveSignalId = registerSignal("lastActiveInterfaceSignal");
        this->switchCountSignalId = registerSignal("switchCountSignal");

        // Register Interface metrics signals
        this->satUsageTimeSignalId = registerSignal("satUsageSignal");
        this->cellUsageTimeSignalId = registerSignal("cellUsageSignal");

        // Initialize counters and timers
        this->totalSwitchesCount = 0;
        this->lastSwitchTime = simTime(); // Initialize to current simulation time
        this->satTotalTime = SimTime::ZERO;
        this->cellTotalTime = SimTime::ZERO;

        // Initialize statistics at start
        emit(this->lastInterfaceActiveSignalId, this->isSatState ? 1 : 0);
        emit(this->switchCountSignalId, this->totalSwitchesCount);
        emit(this->satUsageTimeSignalId, SIMTIME_DBL(this->satTotalTime));
        emit(this->cellUsageTimeSignalId, SIMTIME_DBL(this->cellTotalTime));
        
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
        EV_INFO << "Strategy initialized: " << strategy->getStrategyName() << endl;
    }
}

void HybridInterfaceManager::finish() {
    simtime_t timeSinceLastSwitch = simTime() - this->lastSwitchTime;

    if (this->isSatState) this->satTotalTime += timeSinceLastSwitch;
    else this->cellTotalTime += timeSinceLastSwitch;

    // Emit final statistics
    emit(this->lastInterfaceActiveSignalId, this->isSatState ? 1 : 0);
    emit(this->switchCountSignalId, this->totalSwitchesCount);
    emit(this->satUsageTimeSignalId, SIMTIME_DBL(this->satTotalTime));
    emit(this->cellUsageTimeSignalId, SIMTIME_DBL(this->cellTotalTime));
    EV_INFO << "Finishing: SatTime=" << satTotalTime << ", CellTime=" << cellTotalTime << endl;

    // Call strategy's finish if exists
    if(this->strategy) {
        this->strategy->finish();
    }

    cSimpleModule::finish();
}

ISwitchingStrategy* HybridInterfaceManager::createStrategy() {
    if (switchingMode == "time-based") {
        return new TimeBasedStrategy(this);
    } 
    else if (switchingMode == "coverage-based") {
        return new CoverageBasedStrategy(this);
    }
    else if (switchingMode == "energy-aware") {
        return new EnergyAwareStrategy(this);  
    }
    else if (switchingMode == "qos-based") {
        return new QoSBasedStrategy(this);
    }
    else {
        throw cRuntimeError("Unknown switching mode: %s", switchingMode.c_str());
    }
}

void HybridInterfaceManager::handleMessage(cMessage *msg) {
    if (msg == switchGuardTimerMsg) {
        completeSwitch();
        return;
    }
    if (strategy) {
        strategy->handleMessage(msg);
    } 
    else {
        EV_WARN << "Unexpected message received, ignored." << endl;
        delete msg;
    }
}

void HybridInterfaceManager::performSwitch(bool toSatellite) {
    if (this->isSatState == toSatellite) return;  // No switch needed if already in desired state
    // Update Satellite State with requested state
    this->isSatState = toSatellite;
    // Note: Not used for now
    // if(!switchGuardTimerMsg){
    //     switchGuardTimerMsg = new cMessage("switchGuardTimer");
    // }
    
    // To ensure transimission stability, we set a fixed guard time of 20ms
    // And to ensure that old packets are sent with the old interface before switching
    // scheduleAt(simTime() + 0.02, switchGuardTimerMsg); // 20ms fixed for guard time

    completeSwitch();
}

void HybridInterfaceManager::completeSwitch(){

    simtime_t timeSinceLastSwitch = simTime() - this->lastSwitchTime;

    if (isSatState) this->cellTotalTime += timeSinceLastSwitch; // Update cellular usage time before switch to satellite
    else this->satTotalTime += timeSinceLastSwitch; // Update satellite usage time before switch to cellular
   
    // Update time trackers and counters
    this->totalSwitchesCount++; // Increment switch counter
    this->lastSwitchTime = simTime(); // Update last switch timestamp

    // Emit statistics
    emit(this->lastInterfaceActiveSignalId, this->isSatState ? 1 : 0);
    emit(this->switchCountSignalId, this->totalSwitchesCount);
    emit(this->satUsageTimeSignalId, SIMTIME_DBL(this->satTotalTime));
    emit(this->cellUsageTimeSignalId, SIMTIME_DBL(this->cellTotalTime));

    // Apply the new interface states and check new/old interface carriers
    updateInterfaceStates();
}

void HybridInterfaceManager::updateInterfaceStates() {
    // Satellite Interface State Management
    if (satInterface != nullptr) {
        NetworkInterface::State targetState = isSatState ? NetworkInterface::UP : NetworkInterface::DOWN;
        // 1. ADMINISTRATIVE STATE (UP/DOWN): Check if interface is utilizable for routing or not
        if (satInterface->getState() != targetState) {
            satInterface->setState(targetState);
            EV_DETAIL << "SatInterface State updated to (State=" << (isSatState ? "UP" : "DOWN") 
                      << ", Carrier=" << satInterface->hasCarrier() << ")" << endl;
        }
        // 2. CARRIER STATE (hasCarrier): Check if interface can physically send/receive data
        if (satInterface->hasCarrier() != isSatState) {
            satInterface->setCarrier(isSatState);
            EV_DETAIL << "SatInterface Carrier updated to (State=" << (isSatState ? "UP" : "DOWN") 
                      << ", Carrier=" << satInterface->hasCarrier() << ")" << endl;
        }
    }
    // Cellular Interface State Management
    if (cellInterface != nullptr) {
        bool cellularState = !isSatState;
        // 1. ADMINISTRATIVE STATE (UP/DOWN): Check if interface is utilizable for routing or not
        NetworkInterface::State targetState = cellularState ? NetworkInterface::UP : NetworkInterface::DOWN;
        if (cellInterface->getState() != targetState) {
            cellInterface->setState(targetState);
            EV_DETAIL << "CellInterface State updated to (State=" << (cellularState ? "UP" : "DOWN") 
                      << ", Carrier=" << cellInterface->hasCarrier() << ")" << endl;
        }
        // 2. CARRIER STATE (hasCarrier): Check if interface can physically send/receive data
        if (cellInterface->hasCarrier() != cellularState){
            cellInterface->setCarrier(cellularState);
              EV_DETAIL << "CellInterface Carrier updated to (State=" << (cellularState ? "UP" : "DOWN") 
                      << ", Carrier=" << cellInterface->hasCarrier() << ")" << endl;
        }
    }
}
