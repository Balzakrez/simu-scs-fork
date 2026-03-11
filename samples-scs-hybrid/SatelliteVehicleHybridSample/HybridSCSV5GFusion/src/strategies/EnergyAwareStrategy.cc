//
// EnergyAwareStrategy.cc
//

#include "EnergyAwareStrategy.h"

EnergyAwareStrategy::EnergyAwareStrategy(HybridInterfaceManager *mgr)
    : ISwitchingStrategy(mgr), evaluationTimer(nullptr), lastSwitchTime(SIMTIME_ZERO) {
    evaluationTimer = new cMessage("evaluationTimer");
    currentInterfaceStatistics = {.avgRTT = 0.0, .avgPDR = 0.0, .avgJitter = 0.0, .sampleCount = 0};
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
        manager->emit(currentJitterSignal, 0.0);
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

    // Degradation detection
    minDegradationCount = manager->par("minDegradationCount").intValue();
    
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
        if (!app) throw cRuntimeError("EnergyBasedStrategy: No app module found at index %d", i);
        const char *appType = app->getClassName();
        if (strstr(appType, "UdpBasicApp") != nullptr || strstr(appType, "UdpSink") != nullptr) {
            simsignal_t packetSentSignalId = cComponent::registerSignal("packetSent");
            simsignal_t packetReceivedSignalId = cComponent::registerSignal("packetReceived");
            app->subscribe(packetSentSignalId, this);
            app->subscribe(packetReceivedSignalId, this);
            EV_INFO << "Subscribed to UdpApp signals" << endl;
        }
    }
}

void EnergyAwareStrategy::finish() {
    updateInterfaceStats();

    emitStatistics();

    // Emit final utility score
    double utilityScore = computeUtilityScore();
    manager->emit(utilityScoreSignal, utilityScore);
    manager->emit(qosScoreSignal, computeQoSScore(currentInterfaceStatistics));
    manager->emit(energyEfficiencySignal, computeEnergyEfficiency());
    
    // Total bytes transferred on each interface
    double satTotalBytes  = satelliteTotalTxBytesAccum + satelliteTotalRxBytesAccum;
    double cellTotalBytes = cellularTotalTxBytesAccum  + cellularTotalRxBytesAccum;
    manager->emit(manager->registerSignal("satelliteTotalBytes"), satTotalBytes);
    manager->emit(manager->registerSignal("cellularTotalBytes"), cellTotalBytes);
    
    // Energy consumption statistics (estimated from bytes transferred)
    double satEnergyConsumed = satTotalBytes * satelliteEnergyCostPerByte;
    double cellEnergyConsumed = cellTotalBytes * cellularEnergyCostPerByte;
    double totalEnergyConsumed = satEnergyConsumed + cellEnergyConsumed;
    manager->emit(manager->registerSignal("satelliteEnergyConsumed"), satEnergyConsumed);
    manager->emit(manager->registerSignal("cellularEnergyConsumed"), cellEnergyConsumed);
    manager->emit(manager->registerSignal("totalEnergyConsumed"), totalEnergyConsumed);
    
    // Total bytes transferred across both interfaces
    double totalBytes = satTotalBytes + cellTotalBytes;
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
        double bytes = pkt->getByteLength();
        if (currentInterfaceName == "satellite") { satelliteTotalTxBytesAccum += bytes; } 
        else if (currentInterfaceName == "cellular") { cellularTotalTxBytesAccum += bytes; }
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
        double bytes = pkt->getByteLength();
        if (currentInterfaceName == "satellite") { satelliteTotalRxBytesAccum += bytes; } 
        else if (currentInterfaceName == "cellular") { cellularTotalRxBytesAccum += bytes; }
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
        if (entry.second.usedInterface == currentInterfaceName) {
            sent++;
            if (entry.second.received) {
                totalRTT += (entry.second.rxTime - entry.second.txTime).dbl();
                count++;
            }
        }
    }
    // False positive case: we sent probes but received none, likely indicating very poor conditions (e.g., blackout), 
    // return a high RTT to reflect this in the strategy's decision-making
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
    double currCostPerByte = (currentInterfaceName == "satellite") ? satelliteEnergyCostPerByte : cellularEnergyCostPerByte;
    double minCost = std::min(satelliteEnergyCostPerByte, cellularEnergyCostPerByte);
    double staticEfficiency = (currCostPerByte > 0) ? (minCost / currCostPerByte) : 0.0;

    double residual = energyStorage->getResidualEnergyCapacity().get();
    double nominal = energyStorage->getNominalEnergyCapacity().get();

    double stateOfCharge = (nominal > 0) ? (residual / nominal) : 1.0;
    stateOfCharge = std::max(0.0, std::min(1.0, stateOfCharge));

    // return staticEfficiency * stateOfCharge;
    return staticEfficiency + (1.0 - staticEfficiency) * stateOfCharge;
}

double EnergyAwareStrategy::computeQoSScore(const InterfaceStats &stats) {  
    if (stats.sampleCount < 2) return 0.0; // Insufficient samples
    if(stats.avgPDR == 0.0) return 0.0; // If PDR is 0, QoS is effectively 0 regardless of other metrics
    // Normalize PDR: higher is better -> higher score
    double pdrScore = std::min(1.0, stats.avgPDR / minAcceptablePDR);
    // Normalize RTT: lower is better -> higher score
    double rttScore = 1.0 - std::min(1.0, stats.avgRTT / maxAcceptableRTT);
    // Normalize Jitter: lower is better -> higher score
    double jitterScore = 1.0 - std::min(1.0, stats.avgJitter / maxAcceptableJitter);
    // Weighted average of QoS metrics
    double totalQosWeight = weightRTT + weightPDR + weightJitter;
    // Combined QoS score
    return (rttScore * weightRTT + pdrScore * weightPDR + jitterScore * weightJitter) / totalQosWeight;
}

double EnergyAwareStrategy::computeUtilityScore() {
    double energyEfficiency = computeEnergyEfficiency();
    double qosScore = computeQoSScore(currentInterfaceStatistics);
    double totalUtilityWeight = weightEnergy + weightQos; 
    return (weightEnergy * energyEfficiency + weightQos * qosScore) / totalUtilityWeight;
}


void EnergyAwareStrategy::emitStatistics() {
    manager->emit(currentRTTSignal, currentInterfaceStatistics.avgRTT);
    manager->emit(currentJitterSignal, currentInterfaceStatistics.avgJitter);
    manager->emit(currentPDRSignal, currentInterfaceStatistics.avgPDR);
    double residual = energyStorage->getResidualEnergyCapacity().get();
    manager->emit(residualEnergySignal, residual);
}

void EnergyAwareStrategy::evaluateAndDecide() {
    // 1. Update statistics based on current history and traffic stats
    updateInterfaceStats();

    // 2. Record statistics of updated metric values for current interface
    emitStatistics();

    // 3. Clean old events outside of the measurement window to keep stats relevant and bounded in memory
    cleanOldEvents();

    double residual = energyStorage->getResidualEnergyCapacity().get();
    // Emergency check: if energy is critically low, switch to cellular immediately
    if (residual < criticalEnergyThreshold && currentInterfaceName != "cellular") {
        EV_WARN << "Critical energy (" << residual << "J), forcing cellular." << endl;
        manager->performSwitch(false);
        currentInterfaceName = "cellular";
        lastSwitchTime = simTime();
        consecutiveDegradations = 0; 
        return;
    }

   // 4. Make decision based on updated statistics and thresholds
   performDecision();
}


void EnergyAwareStrategy::performDecision() {

    bool satVisible = isSatelliteVisible();

    // Priority 1: Check reachability - if satellite is not visible, switch to cellular immediately (if not already on it)
    if (currentInterfaceName == "satellite" && !satVisible) {
        manager->performSwitch(false);
        currentInterfaceName = "cellular";
        lastSwitchTime = simTime();
        consecutiveDegradations = 0;
        return;
    }

    // Priority 2: If we don't have enough samples to make a decision, skip evaluation
    if (currentInterfaceStatistics.sampleCount < 2) {
        EV_DETAIL << "Insufficient samples, skipping evaluation." << endl;
        return;
    }

    // Comute and emit current score for the active interface
    double currentUtilityScore = computeUtilityScore();
    manager->emit(utilityScoreSignal, currentUtilityScore);
    manager->emit(qosScoreSignal, computeQoSScore(currentInterfaceStatistics));
    manager->emit(energyEfficiencySignal, computeEnergyEfficiency());

    // Priority 3: If score is below threshold, count degradations and switch if needed
    if (currentUtilityScore < minUtilityScore) {
        consecutiveDegradations++;
        // Check minimum degradation count
        if (consecutiveDegradations < minDegradationCount) {
            EV_DETAIL << "Waiting for " << (minDegradationCount - consecutiveDegradations)
                      << " more degradations before switching." << endl;
            return;
        }
        // Check if minimum hold time has passed since last switch (cooldown)
        if (simTime() - lastSwitchTime < minHoldTime) {
            EV_DETAIL << "In cooldown period." << endl;
            return;
        }
        std::string otherInterface = (currentInterfaceName == "satellite") ? "cellular" : "satellite";
        if (otherInterface == "satellite" && !satVisible) {
            EV_DETAIL << "Satellite not visible, cannot switch." << endl;
            return;
        }
        // Perform the switch
        manager->performSwitch(otherInterface == "satellite");
        // Update current interface
        currentInterfaceName = otherInterface;
        // Update last switch time and reset
        lastSwitchTime = simTime();
        consecutiveDegradations = 0;
    } 
    else {
        consecutiveDegradations = 0;
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