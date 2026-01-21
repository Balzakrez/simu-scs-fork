//
// TimeBasedStrategy.cc - Implementation of time-based switching
//

#include "TimeBasedStrategy.h"

TimeBasedStrategy::TimeBasedStrategy(HybridInterfaceManager *mgr): 
    ISwitchingStrategy(mgr), switchTimer(nullptr) {}
   
TimeBasedStrategy::~TimeBasedStrategy() {
    if (switchTimer) {
        manager->cancelAndDelete(switchTimer);
        switchTimer = nullptr;
    }
}

void TimeBasedStrategy::initialize(int stage) {
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Read parameters
        switchInterval = manager->par("switchInterval");
        
        EV_INFO << "TimeBasedStrategy: switchInterval=" << switchInterval << "s" << endl;
        
        // Start timer
        switchTimer = new cMessage("switchTimer");
        manager->scheduleAt(simTime() + switchInterval, switchTimer); // Initial check, before SatelliteNetworkConfiguratorScs (1.0s)

       EV_INFO << "TimeBasedStrategy initialized. Switching every " << switchInterval << " seconds." << endl;
    }
}

void TimeBasedStrategy::handleMessage(cMessage *msg) {
    // Toggle interface
    if (msg == switchTimer) {
        EV_DETAIL << "Switching interfaces (timer expired)" << endl;
        bool currSatState =  manager->getSatelliteState();
        manager->performSwitch(!currSatState); // Invert current state
        manager->scheduleAt(simTime() + switchInterval, switchTimer); // Reschedule
    } 
    else {
        EV_WARN << "TimeBasedStrategy: unexpected message" << endl;
        delete msg;
    }
}
