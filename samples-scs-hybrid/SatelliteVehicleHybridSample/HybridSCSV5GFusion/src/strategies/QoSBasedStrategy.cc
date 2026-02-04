#include "QoSBasedStrategy.h"

QoSBasedStrategy::QoSBasedStrategy(HybridInterfaceManager *mgr)
   : ISwitchingStrategy(mgr), qosCheckTimerMsg(nullptr), lastSwitchTime(SIMTIME_ZERO) {
      this->qosCheckTimerMsg = new cMessage("qosCheckTimer");
   }  

QoSBasedStrategy::~QoSBasedStrategy() {
   if (qosCheckTimerMsg) { 
      manager->cancelAndDelete(qosCheckTimerMsg); 
   }
   qosCheckTimerMsg = nullptr;
}

void QoSBasedStrategy::handleMessage(cMessage *msg) {
   if (msg == qosCheckTimerMsg) {
      evaluateAndDecide();
      manager->scheduleAt(simTime() + qosCheckInterval, qosCheckTimerMsg);
   }
}

void QoSBasedStrategy::initializeParameters() {
   // Load QoS thresholds parameters
   minAcceptablePDR = manager->par("minAcceptablePDR").doubleValue();
   maxAcceptableDelay = manager->par("maxAcceptableDelay").doubleValue();
   maxAcceptableJitter = manager->par("maxAcceptableJitter").doubleValue();
   minAcceptableThroughput = manager->par("minAcceptableThroughput").doubleValue();
   minQosScore = manager->par("minQosScore").doubleValue();

   // Timing parameters
   minHoldTime = manager->par("minHoldTime").doubleValue();
   qosCheckInterval = manager->par("qosCheckInterval").doubleValue();
   measurementWindowInterval = manager->par("qosMeasurementWindow").doubleValue();
   minDegradationCount = manager->par("minDegradationCount").intValue();

   // Weighted scoring parameters
   weightRTT = manager->par("weightRTT").doubleValue();
   weightPDR = manager->par("weightPDR").doubleValue();
   weightJitter = manager->par("weightJitter").doubleValue();
   weightThroughput = manager->par("weightThroughput").doubleValue();

   // Satellite path
   satModulePath = manager->par("satelliteModulePath").stringValue();
   
   // Get module references
   cModule *hostModule = manager->getParentModule();
   vehicleMobility = check_and_cast<IMobility*>(hostModule->getSubmodule("mobility"));
   
   cModule *satModule = manager->getSimulation()->getSystemModule()->findModuleByPath(satModulePath.c_str());
   if (!satModule) {
      throw cRuntimeError("QoSBasedStrategy: Satellite module not found at path: %s", satModulePath.c_str());
   }
   satMobility = check_and_cast<SatelliteMobilityScs*>(satModule->getSubmodule("mobility"));
   posConverter = check_and_cast<PositionConverter*>(manager->getSimulation()->getSystemModule()->getSubmodule("Pos"));
   
   // Register signals for statistics
   currentRTTSignal = manager->registerSignal("currentRTTSignal");
   currentJitterSignal = manager->registerSignal("currentJitterSignal");
   currentPDRSignal = manager->registerSignal("currentPDRSignal");
   currentThroughputSignal = manager->registerSignal("currentThroughputSignal");

   degradationCountSignal = manager->registerSignal("degradationCountSignal");
   qosScoreSignal = manager->registerSignal("qosScoreSignal");
}

void QoSBasedStrategy::initialize(int stage) {
   if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
      initializeParameters();
      subscribeToApplicationSignals();
      
      // Emit signals for initial statistics
      manager->emit(currentRTTSignal, 0.0);
      manager->emit(currentJitterSignal, 0.0);
      manager->emit(currentPDRSignal, 1.0); // 100% PDR
      manager->emit(currentThroughputSignal, 0.0);
      manager->emit(qosScoreSignal, 1.0); // Perfect QoS score
      manager->emit(degradationCountSignal, 0);

      currentInterface = manager->getSatelliteState() ? "satellite" : "cellular";
      manager->scheduleAt(simTime() + qosCheckInterval, qosCheckTimerMsg);

      EV_INFO << "QoSBasedStrategy initialized. minQosScore: " << minQosScore << ", Current Interface: " << currentInterface << "\n";
   }
}

void QoSBasedStrategy::subscribeToApplicationSignals() {
   cModule *host = manager->getParentModule();
   int numApps = host->par("numApps");
      
   for (int i = 0; i < numApps; i++) {
      cModule *app = host->getSubmodule("app", i);
      if(!app) break;

      const char *appType = app->getClassName();   

      if (strstr(appType, "PingApp") != nullptr) {
         simsignal_t pingTxSeqSignalId = cComponent::registerSignal("pingTxSeq");
         simsignal_t pingRxSeqSignalId = cComponent::registerSignal("pingRxSeq");
         simsignal_t rttSignalId = cComponent::registerSignal("rtt");
         app->subscribe(pingTxSeqSignalId, this);
         app->subscribe(pingRxSeqSignalId, this);
         app->subscribe(rttSignalId, this);
         pingAppModule = app;
      } 
      else if (strstr(appType, "UdpBasicApp") != nullptr || strstr(appType, "UdpSink") != nullptr) {
         simsignal_t packetSentSignalId = cComponent::registerSignal("packetSent");
         simsignal_t packetReceivedSignalId = cComponent::registerSignal("packetReceived");
         app->subscribe(packetSentSignalId, this);
         app->subscribe(packetReceivedSignalId, this);
         udpAppModule = app;  
      }
   }

   if (!pingAppModule && !udpAppModule) {
      throw cRuntimeError("QoSBasedStrategy: No PingApp or UdpBasicApp found!");
   }

}

void QoSBasedStrategy::finish() {
   // Enusure final statistics
   updateInterfaceStats();
   const InterfaceStats &currentStats = (currentInterface == "satellite") ? satelliteStats : cellularStats;
   emitStatistics();
   // Emit final QoS score
   double currentQosScore = computeQoSScore(currentStats);
   manager->emit(qosScoreSignal, currentQosScore);
}

void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, long pingId, cObject *details) {
   const char *signalName = cComponent::getSignalName(signalID);
   simtime_t currentTime = simTime();

   if (strcmp(signalName, "pingTxSeq") == 0){ // Tx
      PingEvent evt {.txTime = currentTime, .responded = false, .interface = this->currentInterface };
      pingHistory[pingId] = evt;
      EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << " Ping TX seq=" << pingId << " , Interface=" << this->currentInterface << std::endl;
   }
   else if (strcmp(signalName, "pingRxSeq") == 0) { // Rx
      auto it = pingHistory.find(pingId);
      if(it != pingHistory.end()){
         it->second.responded = true;
         it->second.rxTime = currentTime;
         EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << " Ping RX seq=" << pingId << " , Interface=" <<this->currentInterface << std::endl;
      }
   } 
}

void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, double d, cObject *details) {
   const char *signalName = cComponent::getSignalName(signalID);
   if (strcmp(signalName, "rtt") == 0) {
      EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << std::endl;
   }
}

void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
   const char *signalName = cComponent::getSignalName(signalID);
   if (strcmp(signalName, "packetSent") == 0) {
      EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << " UDP packet sent " << std::endl;
   }
   else if (strcmp(signalName, "packetReceived") == 0) {
      if (inet::Packet* pkt = dynamic_cast<inet::Packet*>(obj)){
         double bytes = pkt->getByteLength();
         trafficStatsRX.totalBytes += bytes;
         if (trafficStatsRX.startTime == SIMTIME_ZERO) {
            trafficStatsRX.startTime = simTime();
         }
      }
      EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << " UDP packet received " << std::endl;
   }
}

double QoSBasedStrategy::calculateAvgRTT(const std::string &interface) {
   double totalRTT = 0.0;
   int count = 0;
   for (const auto &entry : pingHistory) {
      if (entry.second.responded && entry.second.interface == interface) {
         double rtt = (entry.second.rxTime - entry.second.txTime).dbl();
         totalRTT += rtt;
         count++;
      }
   }
   return (count > 0) ? (totalRTT / count) : 0.0; // Return 0.0 if no samples
}

double QoSBasedStrategy::calculateAvgJitter(const std::string &interface) {
   std::vector<double> rttSamples;
   for (const auto &entry : pingHistory) {
      if (entry.second.responded && entry.second.interface == interface) {
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

double QoSBasedStrategy::calculateAvgPDR(const std::string &interface) {
   int sent = 0;
   int received = 0;
   for (const auto &entry : pingHistory) {
      if (entry.second.interface == interface) {
         sent++;
         if (entry.second.responded) {
               received++;
         }
      }
   }
   return (sent != 0) ? (double)received / (double)sent : 0.0; // Return 0.0 if no packets sent
}

double QoSBasedStrategy::calculateThroughputOnRx() {
   simtime_t currentTime = simTime();
   if(trafficStatsRX.startTime >= currentTime){
      return 0.0;
   }
   simtime_t elapsedTime = currentTime - trafficStatsRX.startTime;
   // bps = (bytes * 8) / seconds
   return (trafficStatsRX.totalBytes * 8.0) / elapsedTime.dbl();
}

void QoSBasedStrategy::updateInterfaceStats() {
   // Update stats for both interfaces
   satelliteStats.avgRTT = calculateAvgRTT("satellite");
   satelliteStats.avgJitter = calculateAvgJitter("satellite");
   satelliteStats.avgPDR = calculateAvgPDR("satellite");
   satelliteStats.lastUpdate = simTime();
   
   cellularStats.avgRTT = calculateAvgRTT("cellular");
   cellularStats.avgJitter = calculateAvgJitter("cellular");
   cellularStats.avgPDR = calculateAvgPDR("cellular");
   cellularStats.lastUpdate = simTime();

   // Update throughput based on interface
   double throughput = calculateThroughputOnRx();
   if (currentInterface == "satellite") {
      satelliteStats.avgThroughput = throughput;
   } 
   else {
      cellularStats.avgThroughput = throughput;
   }

   // Update sample counts
   int satCount = 0, cellCount = 0;
   for (const auto &entry : pingHistory) {
      if (entry.second.interface == "satellite") satCount++;
      else if (entry.second.interface == "cellular") cellCount++;
   }
   satelliteStats.sampleCount = satCount;
   cellularStats.sampleCount = cellCount;
}

void QoSBasedStrategy::cleanOldEvents() {
   // Cleanup Ping History older than measurement window
   simtime_t cutOffTime = simTime() - measurementWindowInterval;    
   
   auto it = pingHistory.begin();
   while (it != pingHistory.end()) {
      if (it->second.txTime < cutOffTime) {
         it = pingHistory.erase(it);
      } 
      else {
         ++it;
      }
   }
   // Reset Traffic Stats for new measurement window
   this->trafficStatsRX = {.totalBytes = 0.0, .startTime = simTime()};
}

bool QoSBasedStrategy::isSatelliteVisible() {
   if (!vehicleMobility || !satMobility || !posConverter) return false;

   Coord pos = vehicleMobility->getCurrentPosition();
   if (std::isnan(pos.x) || std::isnan(pos.y)) return false;

   double vehLon = posConverter->convertPosXToLongitude(pos.x);
   double vehLat = posConverter->convertPosYToLatitude(pos.y);
   return satMobility->isReachable(vehLat, vehLon, 0.0);
}

double QoSBasedStrategy::computeQoSScore(const InterfaceStats &stats) {
   // Normalization Score based on weighted metrics 0-1 (1 = best, 0 = worst)

   if(stats.sampleCount < 2) return 0.0; // Insufficient samples

   // Calculate Normalized RTT in [0,1]
   double rttScore = 1.0 - std::min(1.0, (stats.avgRTT / maxAcceptableDelay));
   
   // Calculate Normalized PDR already in [0,1]
   double pdrScore = stats.avgPDR;
   
   // Calculate Normalized Jitter in [0,1]
   double jitterScore = 1.0 - std::min(1.0, (stats.avgJitter / maxAcceptableJitter));
   
   // Calculate Normalized ThroughputScore in [0,1]
   double throughputScore = std::min(1.0, (stats.avgThroughput / minAcceptableThroughput));
   
   // Total weight (sum must be 1.0)
   double totalWeight = weightRTT + weightPDR + weightJitter + weightThroughput;

   // Compute final score
   double score = (
      rttScore * weightRTT + 
      pdrScore * weightPDR + 
      jitterScore * weightJitter + 
      throughputScore * weightThroughput
   ) / totalWeight;
   
   return score;
}

void QoSBasedStrategy::emitStatistics() {
   const InterfaceStats &stats = (currentInterface == "satellite") ? satelliteStats : cellularStats;
   manager->emit(currentRTTSignal, stats.avgRTT);
   manager->emit(currentJitterSignal, stats.avgJitter);
   manager->emit(currentPDRSignal, stats.avgPDR);
   manager->emit(currentThroughputSignal, stats.avgThroughput);
}

void QoSBasedStrategy::evaluateAndDecide() {
   updateInterfaceStats();
   cleanOldEvents();
   
   EV_DETAIL << "=== QoS Evaluation at t=" << simTime() << "s ===" << endl;
   EV_DETAIL << "  Current interface: " << currentInterface << endl;
   performDecision();
   EV_DETAIL << "======================================" << std::endl;
}

void QoSBasedStrategy::performDecision() {
   const InterfaceStats &currentStats = (currentInterface == "satellite") ? satelliteStats : cellularStats;

   // Record and emit statistics
   emitStatistics();

   // Compute QoS Score and emit signal
   // Ensure statistics are updated before the check if sampleCount < 2 for avoid emitting holes in the statistics
   double currentScore = computeQoSScore(currentStats);
   manager->emit(qosScoreSignal, currentScore);

   // Skip evaluation if insufficient samples
   if (currentStats.sampleCount < 2) {
      EV_DETAIL << "Insufficient samples (" << currentStats.sampleCount << "), skipping evaluation" << std::endl;
      return;
   }

   // Check if the currentScore [0-1] is less than the minimum QoS score threshold
   if (currentScore < minQosScore) {
      EV_DETAIL << "!QoS DEGRADATION detected!" << std::endl;
      consecutiveDegradations++;
      manager->emit(degradationCountSignal, consecutiveDegradations);

      // Debug info
      EV_DETAIL << " PDR: " << (currentStats.avgPDR*100) << " % , minPDR: " << (minAcceptablePDR*100) << " %" << std::endl;
      EV_DETAIL << " RTT: " << (currentStats.avgRTT*1000) << " ms , " << "maxDelay: " << (maxAcceptableDelay*1000) << " ms" << std::endl;
      EV_DETAIL << " Jitter: " << (currentStats.avgJitter*1000) << " ms , maxJitter: " << (maxAcceptableJitter*1000) << " ms" << std::endl;
      EV_DETAIL << " Throughput: " << currentStats.avgThroughput << " bps , minThroughput: " << minAcceptableThroughput << " bps" << std::endl;
      EV_DETAIL << "@SCORE: " << currentScore << " , threshold minQosScore: " << minQosScore << std::endl;
      // End Debug info
      
      // Check minimum degradation count
      if (consecutiveDegradations < minDegradationCount) { // Skip if not enough degradations
         EV_DETAIL << "    Waiting for " << (minDegradationCount - consecutiveDegradations) 
            << " more degradations before switching" << std::endl;
         return;
      }
      
      // Check if minimum hold time has passed since last switch (cooldown)
      if (simTime() - lastSwitchTime < minHoldTime) {
         EV_DETAIL << "    In cooldown period (" << (simTime() - lastSwitchTime) << " < " << minHoldTime << " s)" << std::endl;
         return;
      }

      std::string otherInterface = (currentInterface == "satellite") ? "cellular" : "satellite";
      if(otherInterface == "satellite" && !isSatelliteVisible()){
         EV_DETAIL << "    Satellite not visible, cannot switch to satellite interface." << std::endl;
         return;
      }

      EV_DETAIL << "    SWITCHING from " << currentInterface << " to " << otherInterface << std::endl;
      bool switchToSatellite = (otherInterface == "satellite");
      manager->performSwitch(switchToSatellite);

      // Update current interface
      currentInterface = otherInterface;
      
      // Update current interface, last switch time and emit signal
      lastSwitchTime = simTime();

      //Reset counters and history for new interface
      consecutiveDegradations = 0;
      pingHistory.clear();
      trafficStatsRX = {.totalBytes=0.0, .startTime=simTime()};
      

   } 
   else {
      consecutiveDegradations = 0;
      EV_DETAIL << "#QoS OK: No Action Needed" << std::endl;
   }
}







