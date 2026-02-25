//
// EnergyAwareStrategy.cc
//

#include "EnergyAwareStrategy.h"

EnergyAwareStrategy::EnergyAwareStrategy(HybridInterfaceManager *mgr)
    : ISwitchingStrategy(mgr), evaluationTimer(nullptr), lastSwitchTime(SIMTIME_ZERO) {
    evaluationTimer = new cMessage("evaluationTimer");
    currentInterfaceStatistics = {.avgRTT = 0.0, .avgPDR = 0.0, .avgJitter = 0.0, .sampleCount = 0};
    satelliteRX = {.totalBytes = 0.0, .startTime = SIMTIME_ZERO};
    cellularRX = {.totalBytes = 0.0, .startTime = SIMTIME_ZERO};
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
        manager->emit(qosScoreSignal, 1.0);
        manager->emit(energyEfficiencySignal, 1.0);
        manager->emit(utilityScoreSignal, 1.0); 

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
    
    // Cost models
    satelliteEnergyCostPerByte = manager->par("satelliteEnergyCostPerByte").doubleValue();
    cellularEnergyCostPerByte = manager->par("cellularEnergyCostPerByte").doubleValue();
    
    // Qos thresholds 
    minAcceptablePDR = manager->par("minAcceptablePDR").doubleValue();
    maxAcceptableRTT = manager->par("maxAcceptableRTT").doubleValue();
    maxAcceptableJitter = manager->par("maxAcceptableJitter").doubleValue();
    // Weights for QoS function
    weightPDR = manager->par("weightPDR").doubleValue();
    weightRTT = manager->par("weightRTT").doubleValue();
    weightJitter = manager->par("weightJitter").doubleValue();

    // Weights for utility function
    weightEnergy = manager->par("weightEnergy").doubleValue();
    weightQos = manager->par("weightQos").doubleValue();

    // Utility score threshold
    minUtilityScore = manager->par("minUtilityScore").doubleValue();

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
    currentPDRSignal = manager->registerSignal("currentPDRSignal");
    currentRTTSignal = manager->registerSignal("currentRTTSignal");
    currentJitterSignal = manager->registerSignal("currentJitterSignal");
    residualEnergySignal = manager->registerSignal("residualEnergySignal");
    energyEfficiencySignal = manager->registerSignal("energyEfficiencySignal");
    qosScoreSignal = manager->registerSignal("qosScoreSignal");
    utilityScoreSignal = manager->registerSignal("utilityScoreSignal");
}

void EnergyAwareStrategy::subscribeToApplicationSignals() {
    cModule *host = manager->getParentModule();
    int numApps = host->par("numApps");
    for (int i = 0; i < numApps; i++) {
        cModule *app = host->getSubmodule("app", i);
        if (!app) break; // Skip if no more apps
        const char *appType = app->getClassName();
        if (strstr(appType, "UdpBasicApp") != nullptr || strstr(appType, "UdpSink") != nullptr) {
            simsignal_t packetSentSignalId = cComponent::registerSignal("packetSent");
            simsignal_t packetReceivedSignalId = cComponent::registerSignal("packetReceived");
            app->subscribe(packetSentSignalId, this);
            app->subscribe(packetReceivedSignalId, this);
            udpAppModule = app;
            EV_INFO << "Subscribed to UdpApp signals" << endl;
        }
    }
    if (!udpAppModule) {
        throw cRuntimeError("EnergyAwareStrategy: No UdpBasicApp found!");
    }
}

void EnergyAwareStrategy::finish() {
    updateInterfaceStats();
    double residual = energyStorage->getResidualEnergyCapacity().get();
    manager->emit(residualEnergySignal, residual);
    manager->emit(qosScoreSignal, computeQoSScore());
    manager->emit(energyEfficiencySignal, computeEnergyEfficiency());
    double utilityScore = computeUtilityScore();
    manager->emit(utilityScoreSignal, utilityScore);
    // Energy consumption statistics (estimated from bytes transferred)
    double satEnergyConsumed = satelliteRX.totalBytes * satelliteEnergyCostPerByte;
    double cellEnergyConsumed = cellularRX.totalBytes * cellularEnergyCostPerByte;
    manager->emit(manager->registerSignal("satelliteEnergyConsumed"), satEnergyConsumed);
    manager->emit(manager->registerSignal("cellularEnergyConsumed"), cellEnergyConsumed);
    manager->emit(manager->registerSignal("totalEnergyConsumed"), satEnergyConsumed + cellEnergyConsumed);
    manager->emit(manager->registerSignal("satelliteTotalBytes"), satelliteRX.totalBytes);
    manager->emit(manager->registerSignal("cellularTotalBytes"), cellularRX.totalBytes);
    // Total bytes transferred across both interfaces
    double totalBytes = satelliteRX.totalBytes + cellularRX.totalBytes;
    manager->emit(manager->registerSignal("totalBytesTransferred"), totalBytes);
}

void EnergyAwareStrategy::handleMessage(cMessage *msg) {
    if (msg == evaluationTimer) {
        evaluateAndDecide();
        manager->scheduleAt(simTime() + evaluationInterval, evaluationTimer);
    }
}

void EnergyAwareStrategy::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
    inet::Packet* pkt = dynamic_cast<inet::Packet*>(obj);
    if (!pkt) return; 

    const char *signalName = cComponent::getSignalName(signalID);
    bool isProbe = (std::string(pkt->getName()).find("RTT_Probe") == 0);
    if (strcmp(signalName, "packetSent") == 0) {
        if (isProbe) {
            int seqNum = extractSeqNumber(pkt->getName());
            if (seqNum >= 0) {
                ProbeEvent evt {
                    .txTime = simTime(),
                    .rxTime = SIMTIME_ZERO,
                    .received = false,
                    .usedInterface = currentInterfaceName
                };
                probeHistory[seqNum] = evt;
            }
        }
    }
    else if (strcmp(signalName, "packetReceived") == 0) {
        // Track bytes per interface for energy consumption statistics
        TrafficStats &currTrafficStats = (currentInterfaceName == "satellite") ? satelliteRX : cellularRX;
        currTrafficStats.totalBytes += pkt->getByteLength(); // Add received bytes to current traffic stats
        if (currTrafficStats.startTime == SIMTIME_ZERO) {
            currTrafficStats.startTime = simTime();
        }
        // If this a echo reply for an RTT probe, update the probe history
        if (isProbe) {
            int seqNum = extractSeqNumber(pkt->getName());
            auto it = probeHistory.find(seqNum);
            if (it != probeHistory.end()) {
                it->second.rxTime = simTime();
                it->second.received = true;
            }
        }
    }
}

double EnergyAwareStrategy::calculateAvgRTT() {
    double totalRTT = 0.0;
    int count = 0;
    int sent = 0;
    // Collect RTT samples from probe events for the specified interface
    for (const auto &entry : probeHistory) {
        sent++;
        if (entry.second.received && entry.second.usedInterface == currentInterfaceName) {
            totalRTT += (entry.second.rxTime - entry.second.txTime).dbl();
            count++;
        }
    }
    if (sent > 0 && count == 0) {
        return maxAcceptableRTT * 2.0; 
    }
    return (count > 0) ? (totalRTT / count) : 0.0;
}

double EnergyAwareStrategy::calculateAvgJitter() {
   std::vector<double> rttSamples;
   // Collect RTT samples from probe events for the specified interface
   for (const auto &entry : probeHistory) {
      if (entry.second.received && entry.second.usedInterface == currentInterfaceName) {
         double rtt = (entry.second.rxTime - entry.second.txTime).dbl();
         rttSamples.push_back(rtt);
      }
   }
   if (rttSamples.size() < 2) return 0.0; // Return 0.0 if insufficient samples
   double jitterSum = 0.0;
   for (size_t i = 1; i < rttSamples.size(); i++) {
      jitterSum += fabs(rttSamples[i] - rttSamples[i-1]);
   }
   // Jitter = jitterSum / (N - 1)
   return jitterSum / (rttSamples.size() - 1);
}

double EnergyAwareStrategy::calculateAvgPDR() {
    int sent = 0, received = 0;
    // Calculate PDR from probe events for the specified interface
    for (const auto &entry : probeHistory) {
        if (entry.second.usedInterface == currentInterfaceName) {
            sent++;
            if (entry.second.received) received++;
        }
    }
    return (sent > 0) ? ((double)received / (double)sent) : 1.0; // Assume perfect PDR if no probes were sent to avoid penalizing interfaces without traffic
}

void EnergyAwareStrategy::updateInterfaceStats() {
    double avgRTT = calculateAvgRTT();
    double avgPDR = calculateAvgPDR();
    double avgJitter = calculateAvgJitter();
    currentInterfaceStatistics.avgRTT = avgRTT;
    currentInterfaceStatistics.avgPDR = avgPDR;
    currentInterfaceStatistics.avgJitter = avgJitter;
    // Count samples from probe history for the current interface
    int currSampleCount = 0;
    for (const auto &entry : probeHistory) {          
        if (entry.second.usedInterface == currentInterfaceName) currSampleCount++;
    }
    currentInterfaceStatistics.sampleCount = currSampleCount;
    // Emit updated stats
    manager->emit(currentRTTSignal, avgRTT);
    manager->emit(currentPDRSignal, avgPDR);
    manager->emit(currentJitterSignal, avgJitter);
}

void EnergyAwareStrategy::cleanOldEvents() {
    simtime_t currCutoff = simTime() - cutOffInterval;
    // Cleanup Probe History older than measurement window
    auto itProbe = probeHistory.begin();
    while (itProbe != probeHistory.end()) {
        if (itProbe->second.txTime < currCutoff)
            itProbe = probeHistory.erase(itProbe);
        else
            ++itProbe;
    }
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
    double costPerByte = (currentInterfaceName == "satellite") ? satelliteEnergyCostPerByte : cellularEnergyCostPerByte;
    double minCost = std::min(satelliteEnergyCostPerByte, cellularEnergyCostPerByte);
    // The most expensive interface receives a low score (based on a proportional fraction ≈ 0.2), 
    // while the most efficient interfaces receive a higher score (1.0). 
    return (costPerByte > 0) ? (minCost / costPerByte) : 0.0;
}

double EnergyAwareStrategy::computeQoSScore() {  
    if (currentInterfaceStatistics.sampleCount < 2) return 0.0; // Insufficient samples
    if(currentInterfaceStatistics.avgPDR == 0.0) return 0.0; // If PDR is 0, QoS is effectively 0 regardless of other metrics
    // Normalize PDR: higher is better -> higher score
    double pdrScore = std::min(1.0, currentInterfaceStatistics.avgPDR / minAcceptablePDR);
    // Normalize RTT: lower is better -> higher score
    double rttScore = 1.0 - std::min(1.0, currentInterfaceStatistics.avgRTT / maxAcceptableRTT);
    // Normalize Jitter: lower is better -> higher score
    double jitterScore = 1.0 - std::min(1.0, currentInterfaceStatistics.avgJitter / maxAcceptableJitter);
    // Weighted average of QoS metrics
    double totalQosWeight = weightRTT + weightPDR + weightJitter;
    // Combined QoS score
    return (rttScore * weightRTT + pdrScore * weightPDR + jitterScore * weightJitter) / totalQosWeight;
}

double EnergyAwareStrategy::computeUtilityScore() {
    double energyEfficiency = computeEnergyEfficiency();
    double qosScore = computeQoSScore();
    double totalUtilityWeight = weightEnergy + weightQos; // Sum must be 1.0
    return (weightEnergy * energyEfficiency + weightQos * qosScore) / totalUtilityWeight;
}

void EnergyAwareStrategy::evaluateAndDecide() {
    // 1.  Update statistics  based on current history
    updateInterfaceStats();
    // 2. Emit current energy level
    double residual = energyStorage->getResidualEnergyCapacity().get();
    manager->emit(residualEnergySignal, residual);
    // 3. Emit current utility score for the active interface
    double currUtilityScore = computeUtilityScore();
    manager->emit(utilityScoreSignal, currUtilityScore);
    // 4. Clean old events outside of the measurement window to keep stats relevant and bounded in memory
    cleanOldEvents();

    
    std::string bestInterface = currentInterfaceName; // Default to current
    
    // Priority 1: Critical energy -> use cellular
    if (residual < criticalEnergyThreshold) { 
        bestInterface = "cellular"; 
    }
    // Priority 2: Check cooldown: if recently switched we skip switching
    else if (simTime() - lastSwitchTime < minHoldTime) {
        EV_INFO << "  In cooldown period, skipping switch" << endl;
        return;
    }
    // Priority 3: Check satellite visibility: if satellite is not visible -> use cellular
    else if (!isSatelliteVisible()) { 
        bestInterface = "cellular"; 
    }
    // Priority 4: Utility-based decision (requires sufficient samples)
    else if (currentInterfaceStatistics.sampleCount >= 2) {
        if (currUtilityScore < minUtilityScore) {
            bestInterface = (currentInterfaceName == "satellite") ? "cellular" : "satellite";
        }
    }
    // Priority 5: Default -> stay on current interface
    if (bestInterface != currentInterfaceName) {
        bool switchToSat = (bestInterface == "satellite");
        manager->performSwitch(switchToSat);
        // Update current interface and last switch time
        currentInterfaceName = bestInterface;
        lastSwitchTime = simTime();
        // probeHistory.clear(); // cleanOldEvents already cleans old events based on cutOffInterval
    }
  
}

int EnergyAwareStrategy::extractSeqNumber(const std::string &pktName) {
    size_t pos = pktName.rfind('-');
    if (pos != std::string::npos && pos + 1 < pktName.length()) {
        try {
            return std::stoi(pktName.substr(pos + 1));
        } catch (...) {
            return -1;
        }
    }
    return -1;
}

// void EnergyAwareStrategy::receiveSignal(cComponent *source, simsignal_t signalID, long pingId, cObject *details){
//     const char *signalName = cComponent::getSignalName(signalID);
//     simtime_t currentTime = simTime();   
//     if (strcmp(signalName, "pingTxSeq") == 0) {
//         PingEvent evt{.txTime = currentTime, .rxTime = SIMTIME_ZERO, .responded = false, .pingInterface = currentInterfaceName};
//         pingHistory[pingId] = evt;
//     }
//     else if (strcmp(signalName, "pingRxSeq") == 0) {
//         auto it = pingHistory.find(pingId);
//         if (it != pingHistory.end()) {
//             it->second.responded = true;
//             it->second.rxTime = currentTime;
//         }
//     }
// }