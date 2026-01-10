//
// HybridInterfaceManager.cc - Implementation of hard switching between satellite and cellular interfaces
//
// This module manages time-based interface switching for hybrid satellite/cellular vehicles.
// It toggles between interfaces at configurable intervals, managing carrier states and routes.
//

#include "HybridInterfaceManager.h"

Define_Module(HybridInterfaceManager);

HybridInterfaceManager::~HybridInterfaceManager() {
    cancelAndDelete(switchTimer);
}

void HybridInterfaceManager::initialize(int stage) {
    // INITSTAGE_LOCAL: Load basic parameters (doesn't depend on other modules)
    if (stage == INITSTAGE_LOCAL) {
        switchInterval = par("switchInterval");
        satInterfaceName = par("satInterfaceName").stringValue();
        cellInterfaceName = par("cellInterfaceName").stringValue();
        isSatActive = par("startWithSatellite").boolValue();

        EV_INFO << "INITSTAGE_LOCAL: Parameters loaded. switchInterval=" 
                << switchInterval << "s, startWithSat=" << (isSatActive ? "true" : "false") << endl;
    }
    // INITSTAGE_NETWORK_CONFIGURATION: Interfaces are already registered and configured
    if (stage == INITSTAGE_NETWORK_CONFIGURATION) {
        // Get InterfaceTable
        ptrInterfaceTable = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        if (!ptrInterfaceTable) {
            throw cRuntimeError("InterfaceTable not found!");
        }
        // Get RoutingTable
        cModule *host = getParentModule();
        ptrRoutingTable = dynamic_cast<IIpv4RoutingTable*>(host->getSubmodule("ipv4")->getSubmodule("routingTable"));
        if (!ptrRoutingTable) {
            EV_WARN << "RoutingTable not found, route management disabled." << endl;
        }
        // Find the specific interfaces
        ptrSatInterface = ptrInterfaceTable->findInterfaceByName(satInterfaceName.c_str());
        ptrCellInterface = ptrInterfaceTable->findInterfaceByName(cellInterfaceName.c_str());

        // Error checking (warning only to allow partial simulations)
        if (!ptrSatInterface) {
            EV_WARN << "Satellite Interface '" << satInterfaceName << "' not found." << endl;
        }
        if (!ptrCellInterface) {
            EV_WARN << "Cellular Interface '" << cellInterfaceName << "' not found." << endl;
        }
        // Set initial state
        EV_INFO << "INITSTAGE_NETWORK_CONFIGURATION: Start config: SAT=" 
                << (isSatActive ? "ON" : "OFF") << endl;

        updateInterfaceStates();

        // Start timer only if we have both interfaces (switching makes sense)
        if (ptrSatInterface && ptrCellInterface) {
            switchTimer = new cMessage("switchTimer");
            scheduleAt(simTime() + switchInterval, switchTimer);
            EV_INFO << "Timer started. Next switch @ " << simTime() + switchInterval << endl;
        } else {
            EV_WARN << "Timer NOT started (at least one interface missing)" << endl;
            switchTimer = nullptr;
        }
    }
}

void HybridInterfaceManager::handleMessage(cMessage *msg) {
    if (msg == switchTimer) {
        switchInterfaces();
        scheduleAt(simTime() + switchInterval, switchTimer); // Reschedule next switch
    } else {
        EV_WARN << "Unexpected message received, ignored." << endl;
        delete msg;
    }
}

void HybridInterfaceManager::switchInterfaces() {
    isSatActive = !isSatActive; // Toggle state
    
    EV_INFO << "SWITCHING @ t=" << simTime() 
            << " -> New state: " << (isSatActive ? "SATELLITE" : "CELLULAR") << endl;

    updateInterfaceStates();
    
    // Emit signal for statistics (optional, for post-simulation analysis)
    emit(registerSignal("interfaceSwitch"), isSatActive ? 1 : 0);
}

void HybridInterfaceManager::updateInterfaceStates() {
    // HARD SWITCHING Logic: 
    // Satellite: managed via carrier (to exclude it from routing)
    // Cellular: we do NOT use setCarrier() because it breaks the Simu5G LTE stack
    //           We only manage routes
    
    // ========================================
    // SATELLITE INTERFACE: Managed via carrier
    // The SatelliteNetworkConfigurator will exclude links with carrier=false
    // ========================================
    if (ptrSatInterface) {
        bool currentCarrier = ptrSatInterface->hasCarrier();
        if (currentCarrier != isSatActive) {
            ptrSatInterface->setCarrier(isSatActive);
            EV_DETAIL << satInterfaceName << " carrier -> " << (isSatActive ? "TRUE" : "FALSE") << endl;
        }
    }
    // ========================================
    // CELLULAR INTERFACE: Do NOT touch the carrier!
    // We only manage routes. The configurator preserves cellular routes.
    // ========================================
    if (ptrCellInterface) {
        bool cellState = !isSatActive;
        // Do NOT call setCarrier() on cellular - it breaks Simu5G
        // ptrCellInterface->setCarrier(cellState);
        
        EV_DETAIL << cellInterfaceName << " logical state -> " << (cellState ? "ACTIVE" : "INACTIVE") << endl;
        
        // Manage routes
        updateCellularDefaultRoute(cellState);
        
        // Route cleanup to avoid stale routes from previous phases
        // This ensures that routes are clean at the moment of switching
        if (cellState && ptrSatInterface && ptrRoutingTable) {
            // Switching to CELLULAR: remove all satellite routes
            cleanupSatelliteRoutes();
        } else if (!cellState && ptrSatInterface && ptrRoutingTable) {
            // Switching to SATELLITE: remove specific cellular routes (not the default which was already removed)
            // and remove stale satellite routes that might have been created while carrier was OFF
            cleanupSatelliteRoutes();  // Removes any erroneously created wlan0 routes
            cleanupCellularRoutes();   // Removes specific cellular routes skipping the default route
        }
    }
}

void HybridInterfaceManager::cleanupCellularRoutes() {
    if (!ptrRoutingTable || !ptrCellInterface) return;
    
    int removed = 0;
    for (int i = ptrRoutingTable->getNumRoutes() - 1; i >= 0; i--) {
        Ipv4Route *route = ptrRoutingTable->getRoute(i);
        if (route->getInterface() == ptrCellInterface) {
            // Skip the default route (already removed by updateCellularDefaultRoute)
            if (route->getDestination().isUnspecified() && route->getNetmask().isUnspecified()) {
                continue;
            }
            EV_TRACE << "Cleanup: removing cellular route " 
                     << route->getDestination() << "/" << route->getNetmask()
                     << " via " << cellInterfaceName << endl;
            ptrRoutingTable->deleteRoute(route);
            removed++;
        }
    }
    if (removed > 0) {
        EV_DETAIL << "Cleanup: removed " << removed << " cellular routes" << endl;
    }
}

void HybridInterfaceManager::updateCellularDefaultRoute(bool enable) {

    if (!ptrRoutingTable || !ptrCellInterface) {
        EV_WARN << "Cannot manage cellular route (ptrRoutingTable or ptrCellInterface null)" << endl;
        return;
    }
    EV_DETAIL << "updateCellularDefaultRoute(" << (enable ? "enable" : "disable") 
              << ") - routing table has " << ptrRoutingTable->getNumRoutes() << " routes" << endl;
    
    // Log all routes for debugging
    for (int i = 0; i < ptrRoutingTable->getNumRoutes(); i++) {
        Ipv4Route *route = ptrRoutingTable->getRoute(i);
        EV_TRACE << "  Route " << i << ": dest=" << route->getDestination() 
                 << "/" << route->getNetmask()
                 << " if=" << (route->getInterface() ? route->getInterface()->getInterfaceName() : "null")
                 << " gw=" << route->getGateway() << endl;
    }
    
    if (enable) { 
        // CELLULAR MODE: add default route towards cellular if not existing (enable true: add default route)
        bool routeExists = false;
        for (int i = 0; i < ptrRoutingTable->getNumRoutes(); i++) {
            Ipv4Route *route = ptrRoutingTable->getRoute(i);
            if (route->getInterface() == ptrCellInterface && 
                route->getDestination().isUnspecified() && 
                route->getNetmask().isUnspecified()) {
                routeExists = true;
                EV_DETAIL << "Cellular default route already exists" << endl;
                break;
            }
        }
        if (!routeExists) {
            Ipv4Route *defaultRoute = new Ipv4Route();
            defaultRoute->setDestination(Ipv4Address::UNSPECIFIED_ADDRESS);
            defaultRoute->setNetmask(Ipv4Address::UNSPECIFIED_ADDRESS);
            defaultRoute->setInterface(ptrCellInterface);
            defaultRoute->setSourceType(IRoute::MANUAL);
            ptrRoutingTable->addRoute(defaultRoute);
            EV_INFO << "Cellular default route ADDED" << endl;
        }
    } 
    else {
        // SATELLITE MODE: remove default route towards cellular (the configurator will add the satellite one)
        for (int i = ptrRoutingTable->getNumRoutes() - 1; i >= 0; i--) {
            Ipv4Route *route = ptrRoutingTable->getRoute(i);
            if (route->getInterface() == ptrCellInterface && 
                route->getDestination().isUnspecified() && 
                route->getNetmask().isUnspecified()) {
                ptrRoutingTable->deleteRoute(route);
                EV_INFO << "Cellular default route REMOVED" << endl;
                break;
            }
        }
    }
    
    // Final routes log
    EV_TRACE << "After route update, " << ptrRoutingTable->getNumRoutes() << " routes:" << endl;
    for (int i = 0; i < ptrRoutingTable->getNumRoutes(); i++) {
        Ipv4Route *route = ptrRoutingTable->getRoute(i);
        EV_TRACE << "  Route " << i << ": dest=" << route->getDestination() 
                 << "/" << route->getNetmask()
                 << " if=" << (route->getInterface() ? route->getInterface()->getInterfaceName() : "null")
                 << " gw=" << route->getGateway() << endl;
    }
}

void HybridInterfaceManager::cleanupSatelliteRoutes() {
    // Remove ALL routes that use the satellite interface
    // Called when switching to CELLULAR mode
    int removed = 0;
    for (int i = ptrRoutingTable->getNumRoutes() - 1; i >= 0; i--) {
        Ipv4Route *route = ptrRoutingTable->getRoute(i);
        if (route->getInterface() == ptrSatInterface) {
            EV_TRACE << "Cleanup: removing satellite route " 
                     << route->getDestination() << "/" << route->getNetmask()
                     << " via " << satInterfaceName << endl;
            ptrRoutingTable->deleteRoute(route);
            removed++;
        }
    }
    if (removed > 0) {
        EV_DETAIL << "Cleanup: removed " << removed << " satellite routes" << endl;
    }
}