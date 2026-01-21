// 
// Copyright (C) 2024 Giusepppe Balzano
//
// EnergyBasedStrategy.cc - Energy-based switching implementation
//

#include "EnergyBasedStrategy.h"

EnergyBasedStrategy::EnergyBasedStrategy(HybridInterfaceManager *mgr)
    : ISwitchingStrategy(mgr),
      energyStorage(nullptr),
      energyCheckTimer(nullptr)
{
    this->energyCheckTimer = new cMessage("energyCheckTimer");
}

EnergyBasedStrategy::~EnergyBasedStrategy()
{
    if (this->energyCheckTimer) {
        manager->cancelAndDelete(energyCheckTimer);
        this->energyCheckTimer = nullptr;
    }
}

void EnergyBasedStrategy::initialize(int stage)
{
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Read parameters from manager (that reads from .ned file)
        this->criticalEnergyThreshold = manager->par("criticalEnergyThreshold");
        this->lowEnergyThreshold = manager->par("lowEnergyThreshold");
        this->highEnergyThreshold = manager->par("highEnergyThreshold");
        this->checkInterval = manager->par("energyCheckInterval");
        
        this->satModulePath = manager->par("satelliteModulePath").stringValue();
        
        // Obtain reference to local mobility (Vehicle)
        vehicleMobility = check_and_cast<IMobility*>(manager->getParentModule()->getSubmodule("mobility"));
        
        // Obtain the specific SatelliteMobility module
        // Note: We assume that satModulePath points to the satellite module (e.g., "satellite[0]")
        cModule *satModule = manager->getSimulation()->getSystemModule()->findModuleByPath(satModulePath.c_str());
        if (satModule) {
            satMobility = dynamic_cast<SatelliteMobilityScs*>(satModule->getSubmodule("mobility"));
            if (!satMobility) {
                throw cRuntimeError("EnergyBasedStrategy: SatelliteMobility module not found or invalid at path: %s", satModulePath.c_str());
            } 
        }
        else {
            throw cRuntimeError("EnergyBasedStrategy: Satellite module not found at path: %s", satModulePath.c_str());
        }
        
        posConverter = dynamic_cast<PositionConverter*>(manager->getSimulation()->getSystemModule()->getSubmodule("Pos"));

        if (!posConverter) {
            throw cRuntimeError("EnergyBasedStrategy requires Pos module for coordinate conversion");
        }
        
        // Find energy storage module
        cModule* hostModule = manager->getParentModule();
        energyStorage = dynamic_cast<IEpEnergyStorage*>(hostModule->getSubmodule("energyStorage"));
        
        if (!energyStorage) {
            throw cRuntimeError("EnergyBasedStrategy requires energyStorage submodule");
        }

        // Register strategy-specific signals
        residualEnergySignalId = manager->registerSignal("residualEnergySignal");
                
        EV_INFO << "EnergyBasedStrategy initialized with thresholds: "
                << "Critical=" << criticalEnergyThreshold << "J, "
                << "Low=" << lowEnergyThreshold << "J, "
                << "High=" << highEnergyThreshold << "J, "
                << "CheckInterval=" << checkInterval << "s" << endl;
        
        // Start periodic energy monitoring
        manager->scheduleAt(simTime() + 0.5, energyCheckTimer); // Initial check, before SatelliteNetworkConfiguratorScs (1.0s) 
        
        EV_INFO << "First energy check scheduled in " << checkInterval << "s" << endl;
    }
}

void EnergyBasedStrategy::handleMessage(cMessage *msg)
{
    if (msg == energyCheckTimer) {
        EV_DETAIL << "Energy check timer expired" << endl;
        evaluateAndSwitch();
        
        // Reschedule next check
        manager->scheduleAt(simTime() + checkInterval, energyCheckTimer);
    }
    else {
        EV_WARN << "EnergyBasedStrategy: unexpected message" << endl;
        delete msg;
    }
}

EnergyBasedStrategy::EnergyMode EnergyBasedStrategy::getCurrentEnergyMode()
{
    double residualCapacity = this->energyStorage->getResidualEnergyCapacity().get();
    
    if (residualCapacity < criticalEnergyThreshold) {
        return EnergyMode::CRITICAL;
    } 
    else if (residualCapacity < lowEnergyThreshold) {
        return EnergyMode::LOW;
    } 
    else if (residualCapacity < highEnergyThreshold) {
        return EnergyMode::MEDIUM;
    } 
    else {
        return EnergyMode::HIGH;
    }
}

void EnergyBasedStrategy::evaluateAndSwitch()
{
    EnergyMode currentMode = this->getCurrentEnergyMode();
    double residualCapacity = this->energyStorage->getResidualEnergyCapacity().get();
    bool currentSatState = manager->getSatelliteState();
    bool isSatelliteVisible = this->checkSatelliteVisibility();

    // Emit energy statistics
    manager->emit(this->residualEnergySignalId, residualCapacity);

    EV_INFO << "Energy check: " << residualCapacity << "J" 
            << " | Mode: " << (int)currentMode 
            << " | Satellite visible: " << (isSatelliteVisible ? "YES" : "NO")
            << " | Current: " << (currentSatState ? "SATELLITE" : "CELLULAR") << endl;
    
    // Determine if we need to switch
    switch (currentMode) {
        case EnergyMode::CRITICAL:
            // In this case, enter in LOW case directly
        case EnergyMode::LOW:
            // FORCE/PREFER cellular regardless of satellite visibility
            if (currentSatState) {
                EV_DETAIL << (currentMode == CRITICAL ? "CRITICAL" : "LOW") << " ENERGY! Forcing CELLULAR" << endl;
                manager->performSwitch(false); // false = cellular
            }
            break;
            
        case EnergyMode::MEDIUM:
            // In this case, enter in HIGH case directly
        case EnergyMode::HIGH:
            // CAN USE satellite, BUT ONLY IF VISIBLE
            if (isSatelliteVisible) {
                if (!currentSatState) {
                    EV_DETAIL << "Sufficient energy + Satellite visible → Switching to SATELLITE" << endl;
                    manager->performSwitch(true); // true = satellite
                }
            } else {
                // Satellite not visible → use cellular
                if (currentSatState) {
                    EV_DETAIL << "Satellite NOT visible → Switching to CELLULAR" << endl;
                    manager->performSwitch(false); // false = cellular
                }
            }
            break;
    }
    
}

// Helper method to check satellite visibility
bool EnergyBasedStrategy::checkSatelliteVisibility()
{
     // Pointer safety check
    if (!vehicleMobility || !satMobility || !posConverter) return false;

    // Obtain Cartesian position of the vehicle (as in Configurator)
    Coord pos = vehicleMobility->getCurrentPosition();

    // Coordinate validation
    if (std::isnan(pos.x) || std::isnan(pos.y) || pos.x < 0 || pos.y < 0) {
        return false;
    }

    // Coordinate Conversion: Meters (X,Y) -> Degrees (Lat, Lon)
    double vehLon = posConverter->convertPosXToLongitude(pos.x);
    double vehLat = posConverter->convertPosYToLatitude(pos.y);

    // Calculate Elevation
    // SatelliteMobility exposes getElevation(lat, lon, alt)
    double currentElevation = satMobility->getElevation(vehLat, vehLon, 0.0);
 
    // Decision based on geometric visibility (that use elevation internally)
    if (satMobility->isReachable(vehLat, vehLon, 0.0)) {
        return true;
    }
    return false;

}