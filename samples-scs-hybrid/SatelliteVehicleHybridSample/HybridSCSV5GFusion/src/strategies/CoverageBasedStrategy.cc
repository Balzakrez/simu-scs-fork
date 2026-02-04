//
// CoverageBasedStrategy.cc
// Note: This strategy is a proof-of-concept for demonstration purposes.
// Switching Logic
// Goal: Prefer Satellite when available (or vice versa depending on requirements)
// In this example: Prefer Satellite to offload terrestrial network.
// This example could implement a congested terrestrial network with offloading to satellite when available.
// In the absence of a module that calculates specific 5G/LTE RSSI,
// we assume that the terrestrial network is available ("Always Best Connected" scenario) 
// unless we want to simulate a coverage hole.

#include "CoverageBasedStrategy.h"
#include "inet/common/geometry/common/Coord.h"

// #include "stack/pdcp_rrc/layer/LtePdcpRrc.h"

CoverageBasedStrategy::CoverageBasedStrategy(HybridInterfaceManager *mgr) : 
    ISwitchingStrategy(mgr), checkTimer(nullptr) {}

CoverageBasedStrategy::~CoverageBasedStrategy() {
    if (checkTimer) {
        manager->cancelAndDelete(checkTimer);
    }
}

void CoverageBasedStrategy::initialize(int stage) {
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Load parameters from manager configuration (HybridInterfaceManager.ned)
        // We assume that satModulePath points to the satellite module (e.g., "satellite[0]")
        checkInterval = manager->par("coverageCheckInterval");
        satModulePath = manager->par("satelliteModulePath").stringValue();

        cModule *satModule = manager->getSimulation()->getSystemModule()->findModuleByPath(satModulePath.c_str());

        if (satModule) {
            satMobility = dynamic_cast<SatelliteMobilityScs*>(satModule->getSubmodule("mobility"));
            if (!satMobility) {
                throw cRuntimeError("CoverageBasedStrategy: SatelliteMobility module not found or invalid at path: %s", satModulePath.c_str());
            }
        }
        else {
            throw cRuntimeError("CoverageBasedStrategy: Satellite module not found at path: %s", satModulePath.c_str());
        }
        
        // Obtain reference to local mobility (Vehicle)
        cModule *host = manager->getParentModule();
        vehicleMobility = check_and_cast<IMobility*>(host->getSubmodule("mobility"));
        if (!vehicleMobility) {
            throw cRuntimeError("CoverageBasedStrategy: Vehicle mobility module not found in host module!");
        }

        // Obtain the PositionConverter (essential for conversion)
        posConverter = dynamic_cast<PositionConverter*>(manager->getSimulation()->getSystemModule()->getSubmodule("Pos"));
        if (!posConverter) {
            throw cRuntimeError("CoverageBasedStrategy: Module 'Pos' (PositionConverter) not found in the network!");
        }

        // Start timer
        checkTimer = new cMessage("coverageCheckTimer");
        // manager->scheduleAt(simTime() + checkInterval, checkTimer);
        manager->scheduleAt(simTime() + 0.5, checkTimer); // Initial check, before SatelliteNetworkConfiguratorScs (1.0s)

        //Register signals for statistics
        this->elevationSignalId = manager->registerSignal("satelliteElevationSignal");
        this->coverageLossCountSignalId = manager->registerSignal("coverageLossCountSignal");
        
        this->coverageLossCount = 0; // Initialize counter
       
        EV_INFO << "CoverageBasedStrategy initialized. Checking coverage every " << checkInterval << " seconds." << endl;
    }
}

void CoverageBasedStrategy::handleMessage(cMessage *msg) {
    if (msg == checkTimer) {
        evaluateCoverage();
        manager->scheduleAt(simTime() + checkInterval, checkTimer);
    }
}

bool CoverageBasedStrategy::isSatelliteVisible() {
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
    // double currentElevation = satMobility->getElevation(vehLat, vehLon, 0.0);
 
    // Decision based on geometric visibility (that use elevation internally)
    if (satMobility->isReachable(vehLat, vehLon, 0.0)) {
        return true;
    }
    return false;
}

bool CoverageBasedStrategy::isCellularAvailable() {
    // Todo: Implement more complex logic if needed.
    // Note: Here we keep the logic simple or check the RSSI (Received Signal Strength Indicator) if available.
    // In the absence of a module that calculates specific 5G/LTE RSSI,
    // we assume that the terrestrial network is available ("Always Best Connected" scenario) 
    // unless we want to simulate a coverage hole.
    
    // Check only if the interface physically exists
    NetworkInterface *cellIf = manager->getCellularInterface();
    return (cellIf != nullptr);

    

}

void CoverageBasedStrategy::evaluateCoverage() {
    // Todo: Implement more complex logic if needed.
    // Depending on application requirements, we may prefer one over the other.
    // Switching Logic (Satellite First)
    // Goal: Prefer Satellite when available (or vice versa depending on requirements)
    // This example could implement a congested terrestrial network with offloading to satellite when available.
    // Cellullar is more cheap and low-latency, but satellite offers global coverage.

    // Todo: 1. Check if Hysteresis to avoid frequent switching is already implemented.
    // Todo: 2. Consider Time-to-Trigger (TTT) before switching.
    // Todo: 3. Consider CoolDown periods after a switch (e.g., minimum time between switches).
  
    bool satPhysicallyVisible = this->isSatelliteVisible();
    bool cellAvailable = this->isCellularAvailable();
    bool usingSatellite = manager->getSatelliteState(); // Current interface state

    // Emit elevation if visible 
    if (satPhysicallyVisible) {
        Coord pos = vehicleMobility->getCurrentPosition();
        double vehLat = posConverter->convertPosYToLatitude(pos.y);
        double vehLon = posConverter->convertPosXToLongitude(pos.x);
        double elevation = satMobility->getElevation(vehLat, vehLon, 0.0);
        manager->emit(this->elevationSignalId, elevation);
    }

    if (usingSatellite) {
        // We are using the satellite. 
        if (!satPhysicallyVisible) {
            if (cellAvailable) {
                EV_INFO << "Satellite LOST (Low Elevation/Not Reachable). Switching to CELLULAR..." << endl;
                manager->performSwitch(false); // Go Cellular (false=cellular)

                // Increment coverage loss counter
                this->coverageLossCount++;
                manager->emit(this->coverageLossCountSignalId, this->coverageLossCount);
            }
            else {
                // In this case: we can call performSwitch(false) or not, depending on desired behavior.
                EV_WARN << ">> NO COVERAGE: Satellite lost and Cellular not available!" << endl;
            }
        }
    } 
    else {
        // We are using the cellular network.
        // If the satellite comes back up in the sky, switch back.
        if (satPhysicallyVisible) {
            double currElev = satMobility->getElevation(
                posConverter->convertPosYToLatitude(vehicleMobility->getCurrentPosition().y),
                posConverter->convertPosXToLongitude(vehicleMobility->getCurrentPosition().x), 
                0.0);
            
            EV_INFO << "Satellite FOUND (Elev: " << currElev << "°). Switching to SATELLITE..." << endl;
            manager->performSwitch(true); // Go Satellite (true=satellite)
        }
    }
}
