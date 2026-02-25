#include "QoSBasedStrategy.h"

QoSBasedStrategy::QoSBasedStrategy(HybridInterfaceManager *mgr)
   : ISwitchingStrategy(mgr), qosCheckTimerMsg(nullptr), lastSwitchTime(SIMTIME_ZERO) {
      this->qosCheckTimerMsg = new cMessage("qosCheckTimer");
      currentInterfaceStatistics = {.avgRTT = 0.0, .avgJitter = 0.0, .avgPDR = 1.0, .sampleCount = 0};
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
   maxAcceptableRTT = manager->par("maxAcceptableRTT").doubleValue();
   maxAcceptableJitter = manager->par("maxAcceptableJitter").doubleValue();
   minQosScore = manager->par("minQosScore").doubleValue();

   // Timing parameters
   minHoldTime = manager->par("minHoldTime").doubleValue();
   qosCheckInterval = manager->par("qosCheckInterval").doubleValue();
   cutOffInterval = manager->par("qosCutOffInterval").doubleValue();
   minDegradationCount = manager->par("minDegradationCount").intValue();

   // Weighted scoring parameters
   weightRTT = manager->par("weightRTT").doubleValue();
   weightPDR = manager->par("weightPDR").doubleValue();
   weightJitter = manager->par("weightJitter").doubleValue();

   // Get mobility modules for Vehicle
   cModule *vehicleModule = manager->getParentModule();
   vehicleMobility = check_and_cast<IMobility*>(vehicleModule->getSubmodule("mobility"));
   
   // Get mobility for Ground Station, assuming named MCC[0]
   cModule* gsModule = manager->getSimulation()->getSystemModule()->getSubmodule("MCC", 0); // MCC[0]
   gsMobility = check_and_cast<LUTMotionMobility*>(gsModule->getSubmodule("mobility"));
   
   // Cache PositionConverter
   cModule* posModule = manager->getSimulation()->getSystemModule()->getSubmodule("Pos");
   posConverter = check_and_cast<Satellite::PositionConverter*>(posModule);

   // Register signals for statistics
   currentRTTSignal = manager->registerSignal("currentRTTSignal");
   currentJitterSignal = manager->registerSignal("currentJitterSignal");
   currentPDRSignal = manager->registerSignal("currentPDRSignal");
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
      manager->emit(currentPDRSignal, 1.0); 
      manager->emit(qosScoreSignal, 1.0); 
      manager->emit(degradationCountSignal, 0);

      // Set initial interface state based on manager's satellite state
      currentInterfaceName = manager->getSatelliteState() ? "satellite" : "cellular";

      // Schedule the first evaluation timer
      manager->scheduleAt(simTime() + qosCheckInterval, qosCheckTimerMsg);

      EV_INFO << "QoSBasedStrategy initialized. minQosScore: " << minQosScore << ", Current Interface: " << currentInterfaceName << "\n";
   }
}

void QoSBasedStrategy::subscribeToApplicationSignals() {
   cModule *host = manager->getParentModule();
   // Subscribe to signals from UdpBasicApp
   int numApps = host->par("numApps");
   for (int i = 0; i < numApps; i++) {
      cModule *app = host->getSubmodule("app", i);
      if(!app) break; // Skip if no more apps
      const char *appType = app->getClassName();   
      if (strstr(appType, "UdpBasicApp") != nullptr || strstr(appType, "UdpSink") != nullptr) {
         simsignal_t packetSentSignalId = cComponent::registerSignal("packetSent");
         simsignal_t packetReceivedSignalId = cComponent::registerSignal("packetReceived");
         app->subscribe(packetSentSignalId, this);
         app->subscribe(packetReceivedSignalId, this);
         udpAppModule = app;  
      }
   }
   if (!udpAppModule) {
      throw cRuntimeError("QoSBasedStrategy: No UdpBasicApp found!");
   }

}

void QoSBasedStrategy::finish() {
   updateInterfaceStats();
   emitStatistics();
   double currentQosScore = computeQoSScore(currentInterfaceStatistics);
   manager->emit(qosScoreSignal, currentQosScore);
   
   // Emit total bytes received for each interface
   double satTotalBytes = satelliteTotalRxBytesAccum + satelliteTotalTxBytesAccum;
   double cellTotalBytes = cellularTotalRxBytesAccum + cellularTotalTxBytesAccum;
   manager->emit(manager->registerSignal("satelliteTotalBytes"), satTotalBytes);
   manager->emit(manager->registerSignal("cellularTotalBytes"), cellTotalBytes);
   // Emit total bytes transferred across both interfaces
   double totalBytes = satTotalBytes + cellTotalBytes;
   manager->emit(manager->registerSignal("totalBytesTransferred"), totalBytes);
}

int QoSBasedStrategy::extractSeqNumber(const std::string &pktName) {
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

void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
   inet::Packet* pkt = dynamic_cast<inet::Packet*>(obj);
   if (!pkt) return; // Skip if not a packet
   const char *signalName = cComponent::getSignalName(signalID);
   // Check if this packet is an RTT probe based on its name starting with "RTT_Probe"
   bool isProbe = (std::string(pkt->getName()).find("RTT_Probe") == 0);
   if (strcmp(signalName, "packetSent") == 0) {
      double bytes = pkt->getByteLength();
      // Update cumulative TX counters for final statistics
      if (currentInterfaceName == "satellite") { satelliteTotalTxBytesAccum += bytes; }
      else { cellularTotalTxBytesAccum += bytes; }
      // Only track packets that are RTT probes
      if (isProbe) { 
         int seqNum = extractSeqNumber(pkt->getName());
         if (seqNum >= 0){
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
      // Update cumulative RX counters for final statistics
      if (currentInterfaceName == "satellite") { satelliteTotalRxBytesAccum += bytes; }
      else { cellularTotalRxBytesAccum += bytes; }
      // If thi is a echo reply for an RTT probe, update the probe history
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

double QoSBasedStrategy::calculateAvgRTT() {
   double totalRTT = 0.0;
   int count = 0;
   int sent = 0;
   // Calculate average RTT from probe events for the current interface
   for (const auto &entry : probeHistory) {
      if(entry.second.usedInterface == currentInterfaceName) {
         sent++;
         if (entry.second.received) {
            totalRTT += (entry.second.rxTime - entry.second.txTime).dbl();
            count++;
         }
      }
   }
   //! To avoid blackout 
   if(sent > 0 && count == 0){
      return maxAcceptableRTT * 2.0; // If we sent probes but received none, return a high RTT to reflect poor conditions (e.g., 2x the max acceptable delay)
   } 
   return (count > 0) ? (totalRTT / count) : 0.0; // Return 0.0 if no samples
}

double QoSBasedStrategy::calculateAvgJitter() {
   std::vector<double> rttSamples;
   // Collect RTT samples from probe events for the current interface
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

double QoSBasedStrategy::calculateAvgPDR() {
   int sent = 0;
   int received = 0;
   // Calculate PDR from probe events for the current interface
   for (const auto &entry : probeHistory) {
      if (entry.second.usedInterface == currentInterfaceName) {
         sent++;
         if (entry.second.received) received++;
      }
   }
   return (sent > 0) ? (double)received / (double)sent : 1.0; // Assume perfect PDR if no probes were sent to avoid penalizing interfaces without traffic
}

void QoSBasedStrategy::updateInterfaceStats() {
   currentInterfaceStatistics.avgRTT = calculateAvgRTT();
   currentInterfaceStatistics.avgJitter = calculateAvgJitter();
   currentInterfaceStatistics.avgPDR = calculateAvgPDR();
   currentInterfaceStatistics.lastUpdate = simTime();
   // Update sample counts
   int currSampleCount = 0;
   // Count samples from probe history for the current interface
   for (const auto &entry : probeHistory) {
      if (entry.second.usedInterface == currentInterfaceName) currSampleCount++;
   }
   currentInterfaceStatistics.sampleCount = currSampleCount;
}

void QoSBasedStrategy::cleanOldEvents() {
   simtime_t currCutOffTime = simTime() - cutOffInterval;    
   // Cleanup Probe History older than measurement window
   auto itProbe = probeHistory.begin();
   while (itProbe != probeHistory.end()) {
      if (itProbe->second.txTime < currCutOffTime) {
         itProbe = probeHistory.erase(itProbe);
      } else {
         ++itProbe;
      }
   }
}

bool QoSBasedStrategy::isSatelliteVisible() {
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
         // Check if the satellite can see both the vehicle and the ground station
         if (canSeeVehicle && canSeeGroundStation) { return true; }
      }
   }
   return false;
}

double QoSBasedStrategy::computeQoSScore(const InterfaceStats &stats) {
   if(stats.sampleCount < 2) return 0.0; // Insufficient samples
   if(stats.avgPDR == 0.0) return 0.0; // If PDR is 0, QoS is effectively 0 regardless of other metrics
   // Normalization Score based on weighted metrics 0-1 (1 = best, 0 = worst)
   // Calculate Normalized RTT in [0,1]
   double rttScore = 1.0 - std::min(1.0, (stats.avgRTT / maxAcceptableRTT));
   // Calculate Normalized PDR already in [0,1]
   double pdrScore = std::min(1.0, stats.avgPDR / minAcceptablePDR);
   // Calculate Normalized Jitter in [0,1]
   double jitterScore = 1.0 - std::min(1.0, (stats.avgJitter / maxAcceptableJitter));

   double totalWeight = weightRTT + weightPDR + weightJitter;
   
   // Compute final score
   double score = ( rttScore * weightRTT + pdrScore * weightPDR + jitterScore * weightJitter) / totalWeight;

   return score;
}

void QoSBasedStrategy::emitStatistics() {
   manager->emit(currentRTTSignal, currentInterfaceStatistics.avgRTT);
   manager->emit(currentJitterSignal, currentInterfaceStatistics.avgJitter);
   manager->emit(currentPDRSignal, currentInterfaceStatistics.avgPDR);
}

void QoSBasedStrategy::evaluateAndDecide() {
   // 1. Update statistics based on current history and traffic stats
   updateInterfaceStats();
   // 2. Record statistics of updated metric values for current interface
   emitStatistics();
   // 3. Clean old events outside of the measurement window to keep stats relevant and bounded in memory
   cleanOldEvents();
   // 4. Make decision based on updated statistics and thresholds
   performDecision();
}

void QoSBasedStrategy::performDecision() {
   // Compute QoS Score and emit signal
   double currentScore = computeQoSScore(currentInterfaceStatistics);
   manager->emit(qosScoreSignal, currentScore);
   // Skip evaluation if insufficient samples
   if (currentInterfaceStatistics.sampleCount < 2) {
      EV_DETAIL << "Insufficient samples (" << currentInterfaceStatistics.sampleCount << "), skipping evaluation" << endl;
      return;
   }
   // Check if the currentScore is less than the minimum QoS score threshold
   if (currentScore < minQosScore) {
      consecutiveDegradations++;
      manager->emit(degradationCountSignal, consecutiveDegradations);

      // Check minimum degradation count
      if (consecutiveDegradations < minDegradationCount) { // Skip if not enough degradations
         EV_DETAIL << "    Waiting for " << (minDegradationCount - consecutiveDegradations) 
            << " more degradations before switching" << endl;
         return;
      }
      // Check if minimum hold time has passed since last switch (cooldown)
      if (simTime() - lastSwitchTime < minHoldTime) {
         EV_DETAIL << "    In cooldown period (" << (simTime() - lastSwitchTime) << " < " << minHoldTime << " s)" << endl;
         return;
      }
      std::string otherInterface = (currentInterfaceName == "satellite") ? "cellular" : "satellite";
      if(otherInterface == "satellite" && !isSatelliteVisible()){
         EV_DETAIL << "    Satellite not visible, cannot switch to satellite interface." << endl;
         return;
      }
      bool switchToSatellite = (otherInterface == "satellite");
      manager->performSwitch(switchToSatellite);
      // Update current interface
      currentInterfaceName = otherInterface;
      // Update last switch time and reset
      lastSwitchTime = simTime();
      consecutiveDegradations = 0;
      // probeHistory.clear(); // cleanOldEvents already cleans old events based on cutOffInterval
   } 
   else {
      consecutiveDegradations = 0;
      manager->emit(degradationCountSignal, 0);
   }
}


// double QoSBasedStrategy::calculateThroughputForInterface(const std::string &interface) {
//    simtime_t currentTime = simTime();
//    TrafficStats &stats = (interface == "satellite") ? satelliteTraffic : cellularTraffic;
//    // Check if we have a valid time window to calculate throughput
//    if (stats.startTime >= currentTime || stats.startTime == SIMTIME_ZERO) {
//       return 0.0;
//    }
//    simtime_t elapsedTime = currentTime - stats.startTime;
//    double totalBytes = stats.totalRxBytes + stats.totalTxBytes; // Consider both RX and TX for throughput
//    return (totalBytes * 8.0) / elapsedTime.dbl();
// }

// void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, long pingId, cObject *details) {
//    const char *signalName = cComponent::getSignalName(signalID);
//    simtime_t currentTime = simTime();
//    if (strcmp(signalName, "pingTxSeq") == 0){ // Tx
//       PingEvent evt {.txTime = currentTime, .responded = false, .pingInterface = this->currentInterfaceName };
//       pingHistory[pingId] = evt;
//       EV_DETAIL << "Ping TX seq=" << pingId << " , Interface=" << this->currentInterfaceName << endl;
//    }
//    else if (strcmp(signalName, "pingRxSeq") == 0) { // Rx
//       auto it = pingHistory.find(pingId);
//       if(it != pingHistory.end()){
//          it->second.responded = true;
//          it->second.rxTime = currentTime;
//          EV_DETAIL << "Ping RX seq=" << pingId << " , Interface=" <<this->currentInterfaceName << endl;
//       }
//    } 
// }