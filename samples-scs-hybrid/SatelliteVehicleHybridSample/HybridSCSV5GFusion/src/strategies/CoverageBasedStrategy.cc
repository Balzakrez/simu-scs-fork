//
// CoverageBasedStrategy.cc
// Note: This strategy is a proof-of-concept for demonstration purposes.
// Switching Logic
// Goal: Prefer Satellite when available (or vice versa depending on requirements)
// In this example: Prefer Satellite to offload terrestrial network.
// This example could implement a congested terrestrial network with offloading to satellite when available.

#include "CoverageBasedStrategy.h"
#include "inet/common/geometry/common/Coord.h"
// #include "scs/base/GroundStation.h"
// #include "leosatellites/mobility/GroundStationMobility.h"
// #include "inet/networklayer/configurator/ipv4/SatelliteNetworkConfiguratorScs.h"

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
        checkInterval = manager->par("coverageCheckInterval");
        
        // Cache Vehicle Mobility
        cModule *vehicleModule = manager->getParentModule();
        vehicleMobility = dynamic_cast<IMobility*>(vehicleModule->getSubmodule("mobility"));
        if (!vehicleMobility) {
            throw cRuntimeError("CoverageBasedStrategy: Vehicle mobility module not found in host module!");
        }

        // Cache PositionConverter for coordinate transformations
        posConverter = dynamic_cast<Satellite::PositionConverter*>(manager->getSimulation()->getSystemModule()->getSubmodule("Pos"));
        if (!posConverter) {
            throw cRuntimeError("CoverageBasedStrategy: Module 'Pos' (PositionConverter) not found in the network!");
        }

        cModule* gsModule = manager->getSimulation()->getSystemModule()->getSubmodule("MCC", 0);
        if (!gsModule) {
            throw cRuntimeError("CoverageBasedStrategy: MCC[0] module not found in the network!");
        }
        // Cache ground station mobility for satellite visibility calculations
        gsMobility = dynamic_cast<LUTMotionMobility*>(gsModule->getSubmodule("mobility"));
        if(!gsMobility){
            throw cRuntimeError("CoverageBasedStrategy: Ground Station mobility module not found in MCC[0]!");
        }

        // Start timer
        checkTimer = new cMessage("coverageCheckTimer");
        // manager->scheduleAt(simTime() + checkInterval, checkTimer);
        manager->scheduleAt(simTime() + 0.5, checkTimer); // Initial check, before SatelliteNetworkConfiguratorScs (1.0s)

    
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
    // std::cerr << "\nChecking satellite visibility for node: " << manager->getParentModule()->getFullName() << endl;
   
    if (!vehicleMobility || !posConverter || !gsMobility) return false;
    
    Coord pos = vehicleMobility->getCurrentPosition();
    if (std::isnan(pos.x) || std::isnan(pos.y) || pos.x < 0 || pos.y < 0) {
        return false;
    }

    // Coordinate Conversion: Meters (X,Y) -> Degrees (Lat, Lon)
    double vehLon = posConverter->convertPosXToLongitude(pos.x);
    double vehLat = posConverter->convertPosYToLatitude(pos.y);
    double gsLon = gsMobility->getLUTPositionX();
    double gsLat = gsMobility->getLUTPositionY();

    // Scan all satellites to check if at least one is visible
    // Check if any satellite can see both the vehicle and the ground station
    cModule* network = manager->getSimulation()->getSystemModule();
    for (cModule::SubmoduleIterator it(network); !it.end(); ++it){
        cModule* currSatModule = *it;
        if(std::string(currSatModule->getName()).find("satellite") == 0 ){
            inet::SatelliteMobilityScs* satMob = dynamic_cast<inet::SatelliteMobilityScs*>(currSatModule->getSubmodule("mobility"));
            if(!satMob) continue; // Not a satellite mobility module, skip

            bool canSeeVehicle = satMob->isReachable(vehLat, vehLon, 0.0);
            bool canSeeGroundStation = satMob->isReachable(gsLat, gsLon, 0.0);
                if (canSeeVehicle && canSeeGroundStation) {
                return true;
            }
        }
    }
    return false;
}

bool CoverageBasedStrategy::isCellularAvailable() {
    // Todo: Implement more complex logic if needed.
    EV_DETAIL << "Checking cellular availability for node: " << manager->getParentModule()->getName() << endl;
    NetworkInterface *cellIf = manager->getCellularInterface();
    return (cellIf != nullptr);
}

void CoverageBasedStrategy::evaluateCoverage() {
    // Todo: Implement more complex logic if needed.
    EV_DETAIL << "Evaluating coverage for node: " << manager->getParentModule()->getName() << endl;
    bool cellAvailable = this->isCellularAvailable();
    bool usingSatellite = manager->getSatelliteState(); // Current interface state

    if (usingSatellite) {
        // We are using the satellite. 
        if (!isSatelliteVisible()) {
            if (cellAvailable) {
                EV_WARN << "Satellite LOST (Low Elevation/Not Reachable). Switching to CELLULAR..." << endl;
                manager->performSwitch(false); // Go Cellular (false=cellular)
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
        if (isSatelliteVisible()) {
            EV_WARN << "Satellite FOUND. Switching to SATELLITE..." << endl;
            manager->performSwitch(true); // Go Satellite (true=satellite)
        }
    }
}
