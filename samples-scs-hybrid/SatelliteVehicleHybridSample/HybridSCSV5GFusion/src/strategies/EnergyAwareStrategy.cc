//
// EnergyAwareStrategy.cc
//

#include "EnergyAwareStrategy.h"

EnergyAwareStrategy::EnergyAwareStrategy(HybridInterfaceManager *mgr)
    : ISwitchingStrategy(mgr), evaluationTimer(nullptr), lastSwitchTime(SIMTIME_ZERO) {
    evaluationTimer = new cMessage("evaluationTimer");
    currentStats = {.avgRTT = 0.0, .avgPDR = 0.0, .energyConsumed = 0.0, .sampleCount = 0};
}

EnergyAwareStrategy::~EnergyAwareStrategy() {
    if (evaluationTimer) {
        manager->cancelAndDelete(evaluationTimer);
    }
    evaluationTimer = nullptr;
}

void EnergyAwareStrategy::initialize(int stage) {
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        initializeParameters();
        subscribeToApplicationSignals();
        
        // Emit signals for initial statistics
        manager->emit(currentRTTSignal, 0.0); 
        manager->emit(currentPDRSignal, 1.0);
        manager->emit(residualEnergySignal, energyStorage->getResidualEnergyCapacity().get());
        manager->emit(utilityScoreSignal, 0.5); 

        // Set initial interface state
        currentInterfaceName = manager->getSatelliteState() ? "satellite" : "cellular";

        // Schedule the first evaluation timer
        manager->scheduleAt(simTime() + evaluationInterval, evaluationTimer);
        
        EV_INFO << "EnergyAwareStrategy initialized - Critical threshold: " << criticalEnergyThreshold << "J" << endl;
    }
}

void EnergyAwareStrategy::initializeParameters() {
    // Energy thresholds
    criticalEnergyThreshold = manager->par("criticalEnergyThreshold").doubleValue();
    lowEnergyThreshold = manager->par("lowEnergyThreshold").doubleValue();
    
    // Cost models
    satelliteEnergyCostPerByte = manager->par("satelliteEnergyCostPerByte").doubleValue();
    cellularEnergyCostPerByte = manager->par("cellularEnergyCostPerByte").doubleValue();
    
    // Qos thresholds and utility score threshold
    maxAcceptableRTT = manager->par("maxAcceptableRTT").doubleValue();
    minUtilityScore = manager->par("minUtilityScore").doubleValue();

    // Weights for utility function
    weightEnergy = manager->par("weightEnergy").doubleValue();
    weightQos = manager->par("weightQos").doubleValue();
    
    // Timing parameters
    evaluationInterval = manager->par("energyCheckInterval").doubleValue();
    minHoldTime = manager->par("minHoldTime").doubleValue();
    cutOffInterval = manager->par("energyCutOffInterval").doubleValue();
    
    // Get mobility modules for Vehicle
    cModule *vehicleModule = manager->getParentModule();
    vehicleMobility = check_and_cast<IMobility*>(vehicleModule->getSubmodule("mobility"));

    // Get mobility for Ground Station, assuming named MCC[0]
    cModule* gsModule = manager->getSimulation()->getSystemModule()->getSubmodule("MCC", 0); // MCC[0]
    gsMobility = check_and_cast<LUTMotionMobility*>(gsModule->getSubmodule("mobility"));

    // Cache PositionConverter
    posConverter = check_and_cast<Satellite::PositionConverter*>(manager->getSimulation()->getSystemModule()->getSubmodule("Pos"));
    
    // Cache energy storage
    energyStorage = check_and_cast<IEpEnergyStorage*>(vehicleModule->getSubmodule("energyStorage"));
    
    // Register signals
    currentRTTSignal = manager->registerSignal("currentRTTSignal");
    currentPDRSignal = manager->registerSignal("currentPDRSignal");
    residualEnergySignal = manager->registerSignal("residualEnergySignal");
    utilityScoreSignal = manager->registerSignal("utilityScoreSignal");
}

void EnergyAwareStrategy::subscribeToApplicationSignals() {
    cModule *host = manager->getParentModule();
    int numApps = host->par("numApps");
    
    for (int i = 0; i < numApps; i++) {
        cModule *app = host->getSubmodule("app", i);
        if (!app) break;
        
        const char *appType = app->getClassName();
        if (strstr(appType, "PingApp") != nullptr) {
            simsignal_t pingTxSeq = cComponent::registerSignal("pingTxSeq");
            simsignal_t pingRxSeq = cComponent::registerSignal("pingRxSeq");
            app->subscribe(pingTxSeq, this);
            app->subscribe(pingRxSeq, this);
            pingAppModule = app;
            EV_INFO << "Subscribed to PingApp signals" << endl;
        }
    }
}

void EnergyAwareStrategy::finish() {
    // Update final statistics before finishing
    updateInterfaceStats();
    manager->emit(residualEnergySignal, energyStorage->getResidualEnergyCapacity().get());
    double utilityScore = computeUtilityScore();
    manager->emit(utilityScoreSignal, utilityScore);
}

void EnergyAwareStrategy::handleMessage(cMessage *msg) {
    if (msg == evaluationTimer) {
        evaluateAndDecide();
        manager->scheduleAt(simTime() + evaluationInterval, evaluationTimer);
    }
}

void EnergyAwareStrategy::receiveSignal(cComponent *source, simsignal_t signalID, long pingId, cObject *details){
    const char *signalName = cComponent::getSignalName(signalID);
    simtime_t currentTime = simTime();
    
    if (strcmp(signalName, "pingTxSeq") == 0) {
        PingEvent evt{.txTime = currentTime, .rxTime = SIMTIME_ZERO, .responded = false, .pingInterface = currentInterfaceName};
        pingHistory[pingId] = evt;
    }
    else if (strcmp(signalName, "pingRxSeq") == 0) {
        auto it = pingHistory.find(pingId);
        if (it != pingHistory.end()) {
            it->second.responded = true;
            it->second.rxTime = currentTime;
        }
    }
}

void EnergyAwareStrategy::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
    // Handle packet signals if needed
}

double EnergyAwareStrategy::calculateAvgRTT(){
    double totalRTT = 0.0;
    int count = 0;
    for (const auto &entry : pingHistory) {
        if (entry.second.responded && entry.second.pingInterface == currentInterfaceName) {
            double rtt = (entry.second.rxTime - entry.second.txTime).dbl();
            totalRTT += rtt;
            count++;
        }
    }
    return (count > 0) ? (totalRTT / count) : 0.0;
}

double EnergyAwareStrategy::calculateAvgPDR() {
    int sent = 0, received = 0;
    for (const auto &entry : pingHistory) {
        if (entry.second.pingInterface == currentInterfaceName) {
            sent++;
            if (entry.second.responded) received++;
        }
    }
    return (sent > 0) ? ((double)received / sent) : 1.0;
}

void EnergyAwareStrategy::updateInterfaceStats() {
    double avgRTT = calculateAvgRTT();
    double avgPDR = calculateAvgPDR();
    currentStats.avgRTT = avgRTT;
    currentStats.avgPDR = avgPDR;
    // Update sample counts
    int currSampleCount = 0;
    for (const auto &entry : pingHistory) {
        if (entry.second.pingInterface == currentInterfaceName) currSampleCount++;
    }
    currentStats.sampleCount = currSampleCount;
    // Emit updated stats
    manager->emit(currentRTTSignal, avgRTT);
    manager->emit(currentPDRSignal, avgPDR);
}

void EnergyAwareStrategy::cleanOldPingEvents() {
    simtime_t currCutoff = simTime() - cutOffInterval; 
    auto it = pingHistory.begin();
    while (it != pingHistory.end()) {
        if (it->second.txTime < currCutoff) {
            it = pingHistory.erase(it);
        } 
        else {
            ++it;
        }
    }
}

bool EnergyAwareStrategy::isCriticalEnergy() {
    double residual = energyStorage->getResidualEnergyCapacity().get();
    return (residual < criticalEnergyThreshold);
}

bool EnergyAwareStrategy::isSatelliteVisible() {
    if (!vehicleMobility || !posConverter || !gsMobility) { return false; }

    Coord pos = vehicleMobility->getCurrentPosition();
    if (std::isnan(pos.x) || std::isnan(pos.y) || pos.x < 0 || pos.y < 0) { return false; }

    double vehLon = posConverter->convertPosXToLongitude(pos.x);
    double vehLat = posConverter->convertPosYToLatitude(pos.y);
    double gsLon = gsMobility->getLUTPositionX();
    double gsLat = gsMobility->getLUTPositionY();
   
    // Scan all satellites to check if at least one is visible and can see both the vehicle and the ground station
    cModule* network = manager->getSimulation()->getSystemModule();
    for (cModule::SubmoduleIterator it(network); !it.end(); ++it){
        cModule* currSatModule = *it;
        if(std::string(currSatModule->getName()).find("satellite") == 0 ){
            inet::SatelliteMobilityScs* satMob = dynamic_cast<inet::SatelliteMobilityScs*>(currSatModule->getSubmodule("mobility"));
            if(!satMob) continue; // Not a satellite mobility module, skip
            bool canSeeVehicle = satMob->isReachable(vehLat, vehLon, 0.0);
            bool canSeeGroundStation = satMob->isReachable(gsLat, gsLon, 0.0);
            // Check if this satellite can see both the vehicle and the ground station
            if (canSeeVehicle && canSeeGroundStation) { return true; }
        }
    }
    return false;
}

double EnergyAwareStrategy::computeEnergyEfficiency() {
    // Normalized energy cost: higher value = worse
    double costPerByte = (currentInterfaceName == "satellite") ? satelliteEnergyCostPerByte : cellularEnergyCostPerByte;
    // Normalize to [0, 1] where 0 is best
    double maxCost = std::max(satelliteEnergyCostPerByte, cellularEnergyCostPerByte);
    return (maxCost > 0) ? (1 - costPerByte / maxCost) : 0.0; // [0,1] where 1 is best
}

double EnergyAwareStrategy::computeQoSScore() {  
    if (currentStats.sampleCount < 2) return 0.0; // Insufficient samples
    // Normalize RTT: lower is better -> higher score
    double rttScore = 1.0 - std::min(1.0, currentStats.avgRTT / maxAcceptableRTT);
    // PDR already in [0,1], higher is better
    double pdrScore = currentStats.avgPDR;
    // Combined QoS score
    return (rttScore + pdrScore) / 2.0; // [0,1] where 1 is best
}

double EnergyAwareStrategy::computeUtilityScore() {
    double energyEfficiency = computeEnergyEfficiency();
    double qosScore = computeQoSScore();
    double totalWeight = weightEnergy + weightQos;
    return (weightEnergy * energyEfficiency + weightQos * qosScore) / totalWeight;
}

void EnergyAwareStrategy::evaluateAndDecide() {
    // 1.  Update statistics  based on current ping history
    updateInterfaceStats();
    // 2. Emit current energy level
    double residual = energyStorage->getResidualEnergyCapacity().get();
    manager->emit(residualEnergySignal, residual);
    // 3. Emit current utility score for the active interface (
    double currUtilityScore = computeUtilityScore();
    manager->emit(utilityScoreSignal, currUtilityScore);
    // 4. Clean old events outside of the measurement window to keep stats relevant and bounded in memory
    cleanOldPingEvents();
    
    // Check cooldown: if recently switched we skip switching
    if (simTime() - lastSwitchTime < minHoldTime) {
        EV_INFO << "  In cooldown period, skipping switch" << endl;
        return;
    }
   
    std::string bestInterface = currentInterfaceName; // Default to current
    // 1. Critical energy -> cellular (Priority 1)
    if (residual < criticalEnergyThreshold) { 
        bestInterface = "cellular"; 
    }
    // 2. Satellite not visible -> cellular (Priority 2)
    else if (!isSatelliteVisible()) { 
        bestInterface = "cellular"; 
    }
    // 3. Utility-based decision (Priority 3))
    else if (currentStats.sampleCount >= 5) {
        double utilityScore = computeUtilityScore();
        if (utilityScore < minUtilityScore) {
            bestInterface = (currentInterfaceName == "satellite") ? "cellular" : "satellite";
        }
    }
    // 4. Default: stay on current interface

    if (bestInterface != currentInterfaceName) {
        bool switchToSat = (bestInterface == "satellite");
        manager->performSwitch(switchToSat);
        // Update current interface and last switch time
        currentInterfaceName = bestInterface;
        lastSwitchTime = simTime();
        // pingHistory.clear(); // Optionally clear history on switch
    }
  
}
