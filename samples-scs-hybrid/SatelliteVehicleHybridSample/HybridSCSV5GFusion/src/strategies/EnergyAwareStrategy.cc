//
// EnergyAwareStrategy.cc
//

#include "EnergyAwareStrategy.h"

EnergyAwareStrategy::EnergyAwareStrategy(HybridInterfaceManager *mgr)
    : ISwitchingStrategy(mgr), evaluationTimer(nullptr), lastSwitchTime(SIMTIME_ZERO)
{
    evaluationTimer = new cMessage("evaluationTimer");
    satelliteStats = {.avgRTT = 0.0, .avgPDR = 0.0, .energyConsumed = 0.0, .sampleCount = 0};
    cellularStats = {.avgRTT = 0.0, .avgPDR = 0.0, .energyConsumed = 0.0, .sampleCount = 0};
}

EnergyAwareStrategy::~EnergyAwareStrategy()
{
    if (evaluationTimer) {
        manager->cancelAndDelete(evaluationTimer);
    }
    evaluationTimer = nullptr;
}

void EnergyAwareStrategy::initialize(int stage)
{
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        initializeParameters();
        subscribeToApplicationSignals();
        
        // Emit signals for initial statistics
        manager->emit(currentRTTSignal, 0.0); 
        manager->emit(currentPDRSignal, 1.0); // 100 % 
        if(energyStorage){
            manager->emit(residualEnergySignal, energyStorage->getResidualEnergyCapacity().get());
        }
        manager->emit(utilityScoreSignal, 0.5); // Neutral initial score

        // Start periodic evaluation
        manager->scheduleAt(simTime() + evaluationInterval, evaluationTimer);
        
        EV_INFO << "EnergyAwareStrategy initialized - Mode: " << (int)optimizationMode 
                << ", Critical threshold: " << criticalEnergyThreshold << "J" << endl;
    }
}

void EnergyAwareStrategy::initializeParameters()
{
    // Optimization mode
    std::string modeStr = manager->par("optimizationMode").stringValue();
    if (modeStr == "minimize_energy") optimizationMode = MINIMIZE_ENERGY;
    else if (modeStr == "maximize_qos") optimizationMode = MAXIMIZE_QOS;
    else optimizationMode = BALANCED;
    
    // Energy thresholds
    criticalEnergyThreshold = manager->par("criticalEnergyThreshold").doubleValue();
    lowEnergyThreshold = manager->par("lowEnergyThreshold").doubleValue();
    
    // Cost models
    satelliteEnergyCostPerByte = manager->par("satelliteEnergyCostPerByte").doubleValue();
    cellularEnergyCostPerByte = manager->par("cellularEnergyCostPerByte").doubleValue();
    
    // QoS thresholds
    minAcceptableRTT = manager->par("minAcceptableRTT").doubleValue();
    maxAcceptableRTT = manager->par("maxAcceptableRTT").doubleValue();
    minAcceptablePDR = manager->par("minAcceptablePDR").doubleValue();
    
    // Timing
    evaluationInterval = manager->par("energyCheckInterval").doubleValue();
    minHoldTime = manager->par("minHoldTime").doubleValue();
    
    // Satellite path
    satModulePath = manager->par("satelliteModulePath").stringValue();
    
    // Get references to modules
    cModule *hostModule = manager->getParentModule();
    energyStorage = dynamic_cast<IEpEnergyStorage*>(hostModule->getSubmodule("energyStorage"));
    vehicleMobility = check_and_cast<IMobility*>(hostModule->getSubmodule("mobility"));
    
    cModule *satModule = manager->getSimulation()->getSystemModule()->findModuleByPath(satModulePath.c_str());
    if (satModule) {
        satMobility = dynamic_cast<SatelliteMobilityScs*>(satModule->getSubmodule("mobility"));
    }
    
    posConverter = dynamic_cast<PositionConverter*>(
        manager->getSimulation()->getSystemModule()->getSubmodule("Pos"));
    
    if (!energyStorage || !satMobility || !posConverter) {
        throw cRuntimeError("EnergyAwareStrategy: Required modules not found");
    }
    
    // Current interface
    currentInterface = manager->getSatelliteState() ? "satellite" : "cellular";
    
    // Register signals
    currentRTTSignal = manager->registerSignal("currentRTTSignal");
    currentPDRSignal = manager->registerSignal("currentPDRSignal");

    residualEnergySignal = manager->registerSignal("residualEnergySignal");
    utilityScoreSignal = manager->registerSignal("utilityScoreSignal");
}

void EnergyAwareStrategy::subscribeToApplicationSignals()
{
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

void EnergyAwareStrategy::finish(){
    // Final update of interface statistics
    updateInterfaceStats();

    if(energyStorage){
        manager->emit(residualEnergySignal, energyStorage->getResidualEnergyCapacity().get());
    }
    double utilityScore = computeUtilityScore(currentInterface);
    manager->emit(utilityScoreSignal, utilityScore);
    EV_INFO << "EnergyAwareStrategy: Final statistics recorded." << endl;
}

void EnergyAwareStrategy::handleMessage(cMessage *msg)
{
    if (msg == evaluationTimer) {
        evaluateAndDecide();
        manager->scheduleAt(simTime() + evaluationInterval, evaluationTimer);
    }
}

void EnergyAwareStrategy::receiveSignal(cComponent *source, simsignal_t signalID, long pingId, cObject *details)
{
    const char *signalName = cComponent::getSignalName(signalID);
    simtime_t currentTime = simTime();
    
    if (strcmp(signalName, "pingTxSeq") == 0) {
        PingEvent evt{.txTime = currentTime, .rxTime = SIMTIME_ZERO, .responded = false, .interface = currentInterface};
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

void EnergyAwareStrategy::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    // Handle packet signals if needed
}

double EnergyAwareStrategy::calculateAvgRTT()
{
    double totalRTT = 0.0;
    int count = 0;
    
    for (const auto &entry : pingHistory) {
        if (entry.second.responded && entry.second.interface == currentInterface) {
            double rtt = (entry.second.rxTime - entry.second.txTime).dbl();
            totalRTT += rtt;
            count++;
        }
    }
    
    return (count > 0) ? (totalRTT / count) : 0.0;
}

double EnergyAwareStrategy::calculateAvgPDR()
{
    int sent = 0, received = 0;
    
    for (const auto &entry : pingHistory) {
        if (entry.second.interface == currentInterface) {
            sent++;
            if (entry.second.responded) received++;
        }
    }
    
    return (sent > 0) ? ((double)received / sent) : 1.0;
}

void EnergyAwareStrategy::updateInterfaceStats() 
{
    double avgRTT = calculateAvgRTT();
    double avgPDR = calculateAvgPDR();
    
    if (currentInterface == "satellite") {
        satelliteStats.avgRTT = avgRTT;
        satelliteStats.avgPDR = avgPDR;
        satelliteStats.sampleCount = pingHistory.size();
    } 
    else {
        cellularStats.avgRTT = avgRTT;
        cellularStats.avgPDR = avgPDR;
        cellularStats.sampleCount = pingHistory.size();
    }
    
    manager->emit(currentRTTSignal, avgRTT);
    manager->emit(currentPDRSignal, avgPDR);
}

void EnergyAwareStrategy::cleanOldPingEvents() 
{
    simtime_t cutoff = simTime() - 10.0; // Keep last 10 seconds
    auto it = pingHistory.begin();
    while (it != pingHistory.end()) {
        if (it->second.txTime < cutoff) {
            it = pingHistory.erase(it);
        } 
        else {
            ++it;
        }
    }
}

bool EnergyAwareStrategy::isCriticalEnergy()
{
    double residual = energyStorage->getResidualEnergyCapacity().get();
    return (residual < criticalEnergyThreshold);
}

bool EnergyAwareStrategy::isSatelliteVisible()
{
    if (!vehicleMobility || !satMobility || !posConverter) return false;
    
    Coord pos = vehicleMobility->getCurrentPosition();
    if (std::isnan(pos.x) || std::isnan(pos.y)) return false;
    
    double vehLon = posConverter->convertPosXToLongitude(pos.x);
    double vehLat = posConverter->convertPosYToLatitude(pos.y);
    
    return satMobility->isReachable(vehLat, vehLon, 0.0);
}

double EnergyAwareStrategy::computeEnergyCost(const std::string &interface)
{
    // Normalized energy cost: higher value = worse
    double costPerByte = (interface == "satellite") ? 
        satelliteEnergyCostPerByte : cellularEnergyCostPerByte;
    
    // Normalize to [0, 1] where 0 is best
    double maxCost = std::max(satelliteEnergyCostPerByte, cellularEnergyCostPerByte);
    return (maxCost > 0) ? (costPerByte / maxCost) : 0.0;
}

double EnergyAwareStrategy::computeQoSScore(const std::string &interface)
{
    // Get stats for the interface
    const InterfaceStats &stats = (interface == "satellite") ? satelliteStats : cellularStats;
    
    if (stats.sampleCount < 2) return 0.5; //! Unknown, assume medium
    
    // Normalize RTT: lower is better -> higher score
    double rttScore = 1.0 - std::min(1.0, stats.avgRTT / maxAcceptableRTT);
    
    // PDR already in [0,1], higher is better
    double pdrScore = stats.avgPDR;
    
    // Combined QoS score
    return (rttScore + pdrScore) / 2.0;
}

double EnergyAwareStrategy::computeUtilityScore(const std::string &interface)
{
    double energyCost = computeEnergyCost(interface);
    double qosScore = computeQoSScore(interface);
    
    double utility = 0.0;
    
    switch (optimizationMode) {
        case MINIMIZE_ENERGY:
            // Minimize energy cost (invert so higher is better)
            utility = 1.0 - energyCost;
            break;
            
        case MAXIMIZE_QOS:
            // Maximize QoS
            utility = qosScore;
            break;
            
        case BALANCED:
            // Balance: 50% energy, 50% QoS
            utility = 0.5 * (1.0 - energyCost) + 0.5 * qosScore;
            break;
    }

    return utility;
}

std::string EnergyAwareStrategy::getBestInterface()
{
    double satUtility = computeUtilityScore("satellite");
    double cellUtility = computeUtilityScore("cellular");
    
    // Add visibility constraint for satellite
    if (!isSatelliteVisible()) {
        satUtility = 0.0; // Can't use satellite if not visible
    }
    
    EV_INFO << "Utility scores - Satellite: " << satUtility << ", Cellular: " << cellUtility << endl;
    
    return (satUtility > cellUtility) ? "satellite" : "cellular";
}

void EnergyAwareStrategy::evaluateAndDecide()
{
    double residual = energyStorage->getResidualEnergyCapacity().get();
    manager->emit(residualEnergySignal, residual);
    
    // Update statistics
    updateInterfaceStats();
    cleanOldPingEvents();
    
    EV_INFO << "=== Energy-Aware Evaluation at t=" << simTime() << "s ===" << endl;
    EV_INFO << "  Residual energy: " << residual << "J" << endl;
    EV_INFO << "  Current interface: " << currentInterface << endl;
    
    // Critical energy: force cellular regardless of mode
    if (isCriticalEnergy()) {
        EV_WARN << "CRITICAL ENERGY! Forcing cellular interface" << endl;

        double forcedUtility = computeUtilityScore("cellular");
        manager->emit(utilityScoreSignal, forcedUtility);

        if (currentInterface == "satellite") {
            manager->performSwitch(false);
            currentInterface = "cellular";
            lastSwitchTime = simTime();
            pingHistory.clear();
        }
        return;
    }
    
    // Check cooldown: if recently switched we skip switching
    if (simTime() - lastSwitchTime < minHoldTime) {
        EV_INFO << "  In cooldown period, skipping switch" << endl;
        double cooldownUtility = computeUtilityScore(currentInterface);
        manager->emit(utilityScoreSignal, cooldownUtility);
        return;
    }
    
    // Determine best interface based on utility
    std::string bestInterface = getBestInterface();
    double chosenUtility = computeUtilityScore(bestInterface);
    manager->emit(utilityScoreSignal, chosenUtility);
    
    if (bestInterface != currentInterface) {
        EV_INFO << "  Switching to " << bestInterface << endl;
        bool switchToSat = (bestInterface == "satellite");
        manager->performSwitch(switchToSat);
        currentInterface = bestInterface;
        lastSwitchTime = simTime();
        pingHistory.clear(); // Reset stats for new interface
    } 
    else {
        EV_INFO << "  Staying on " << currentInterface << endl;
    }
}