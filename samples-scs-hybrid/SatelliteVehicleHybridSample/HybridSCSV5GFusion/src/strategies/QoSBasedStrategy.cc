#include "QoSBasedStrategy.h"

QoSBasedStrategy::QoSBasedStrategy(HybridInterfaceManager *mgr)
   : ISwitchingStrategy(mgr), qosCheckTimerMsg(nullptr), lastSwitchTime(SIMTIME_ZERO) {
      this->qosCheckTimerMsg = new cMessage("qosCheckTimer");
      currentStats = {.avgRTT = 0.0, .avgJitter = 0.0, .avgPDR = 1.0, .avgThroughput = 0.0, .sampleCount = 0};
      satelliteRX = {.totalBytes = 0.0, .startTime = SIMTIME_ZERO};
      cellularRX = {.totalBytes = 0.0, .startTime = SIMTIME_ZERO};
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
   cutOffInterval = manager->par("qosCutOffInterval").doubleValue();
   minDegradationCount = manager->par("minDegradationCount").intValue();

   // Weighted scoring parameters
   weightRTT = manager->par("weightRTT").doubleValue();
   weightPDR = manager->par("weightPDR").doubleValue();
   weightJitter = manager->par("weightJitter").doubleValue();
   weightThroughput = manager->par("weightThroughput").doubleValue();

   // Get mobility modules for Vehicle
   cModule *vehicleModule = manager->getParentModule();
   vehicleMobility = check_and_cast<IMobility*>(vehicleModule->getSubmodule("mobility"));
   
   // Get mobility for Ground Station, assuming named MCC[0]
   cModule* gsModule = manager->getSimulation()->getSystemModule()->getSubmodule("MCC", 0); // MCC[0]
   gsMobility = check_and_cast<LUTMotionMobility*>(gsModule->getSubmodule("mobility"));
   
   // Cache PositionConverter
   posConverter = check_and_cast<Satellite::PositionConverter*>(manager->getSimulation()->getSystemModule()->getSubmodule("Pos"));

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
      manager->emit(currentPDRSignal, 1.0); 
      manager->emit(currentThroughputSignal, 0.0);
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
   // Update final statistics before finishing
   updateInterfaceStats();
   emitStatistics();
   double currentQosScore = computeQoSScore(currentStats);
   manager->emit(qosScoreSignal, currentQosScore);
   // Emit satellite cumulative statistics
   double satThroughput = calculateThroughputForInterface("satellite");
   double satRTT = calculateAvgRTT("satellite");
   double satPDR = calculateAvgPDR("satellite");
   double satJitter = calculateAvgJitter("satellite");
   manager->emit(manager->registerSignal("satelliteCumulativeThroughput"), satThroughput);
   manager->emit(manager->registerSignal("satelliteCumulativeRTT"), satRTT);
   manager->emit(manager->registerSignal("satelliteCumulativePDR"), satPDR);
   manager->emit(manager->registerSignal("satelliteCumulativeJitter"), satJitter);
   manager->emit(manager->registerSignal("satelliteTotalBytes"), satelliteRX.totalBytes);
   // Emit cellular cumulative statistics
   double cellThroughput = calculateThroughputForInterface("cellular");
   double cellRTT = calculateAvgRTT("cellular");
   double cellPDR = calculateAvgPDR("cellular");
   double cellJitter = calculateAvgJitter("cellular");
   manager->emit(manager->registerSignal("cellularCumulativeThroughput"), cellThroughput);
   manager->emit(manager->registerSignal("cellularCumulativeRTT"), cellRTT);
   manager->emit(manager->registerSignal("cellularCumulativePDR"), cellPDR);
   manager->emit(manager->registerSignal("cellularCumulativeJitter"), cellJitter);
   manager->emit(manager->registerSignal("cellularTotalBytes"), cellularRX.totalBytes);
   // Total bytes transferred across both interfaces
   double totalBytes = satelliteRX.totalBytes + cellularRX.totalBytes;
   manager->emit(manager->registerSignal("totalBytesTransferred"), totalBytes);
}


void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, long pingId, cObject *details) {
   const char *signalName = cComponent::getSignalName(signalID);
   simtime_t currentTime = simTime();

   if (strcmp(signalName, "pingTxSeq") == 0){ // Tx
      PingEvent evt {.txTime = currentTime, .responded = false, .pingInterface = this->currentInterfaceName };
      pingHistory[pingId] = evt;
      EV_DETAIL << "Ping TX seq=" << pingId << " , Interface=" << this->currentInterfaceName << endl;
   }
   else if (strcmp(signalName, "pingRxSeq") == 0) { // Rx
      auto it = pingHistory.find(pingId);
      if(it != pingHistory.end()){
         it->second.responded = true;
         it->second.rxTime = currentTime;
         EV_DETAIL << "Ping RX seq=" << pingId << " , Interface=" <<this->currentInterfaceName << endl;
      }
   } 
}

void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, double d, cObject *details) {
   const char *signalName = cComponent::getSignalName(signalID);
   if (strcmp(signalName, "rtt") == 0) {
      EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << endl;
   }
}

void QoSBasedStrategy::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
   const char *signalName = cComponent::getSignalName(signalID);
   if (strcmp(signalName, "packetSent") == 0) {
      EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << " UDP packet sent " << endl;
   }
   else if (strcmp(signalName, "packetReceived") == 0) {
      if (inet::Packet* pkt = dynamic_cast<inet::Packet*>(obj)){
         double bytes = pkt->getByteLength();
         // Get the current interface's traffic stats reference
         TrafficStats &currTrafficStats = (currentInterfaceName == "satellite") ? satelliteRX : cellularRX;
         currTrafficStats.totalBytes += bytes; // Update total bytes on current interface
         if (currTrafficStats.startTime == SIMTIME_ZERO) {
            currTrafficStats.startTime = simTime();
         }
      }
      EV_DETAIL << "DEBUG: " << manager->getParentModule()->getFullName() << " UDP packet received " << endl;
   }
}

double QoSBasedStrategy::calculateAvgRTT(const std::string &interface) {
   double totalRTT = 0.0;
   int count = 0;
   for (const auto &entry : pingHistory) {
      if (entry.second.responded && entry.second.pingInterface == interface) {
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
      if (entry.second.responded && entry.second.pingInterface == interface) {
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
      if (entry.second.pingInterface == interface) {
         sent++;
         if (entry.second.responded) {
               received++;
         }
      }
   }
   return (sent != 0) ? (double)received / (double)sent : 0.0; // Return 0.0 if no packets sent
}

double QoSBasedStrategy::calculateThroughputOnRx() {
   return calculateThroughputForInterface(currentInterfaceName);
}

double QoSBasedStrategy::calculateThroughputForInterface(const std::string &interface) {
   simtime_t currentTime = simTime();
   // Get the appropriate traffic stats reference based on the interface
   TrafficStats &stats = (interface == "satellite") ? satelliteRX : cellularRX;
   if(stats.startTime >= currentTime || stats.startTime == SIMTIME_ZERO){
      return 0.0;
   }
   simtime_t elapsedTime = currentTime - stats.startTime;
   return (stats.totalBytes * 8.0) / elapsedTime.dbl();
}

void QoSBasedStrategy::updateInterfaceStats() {
   currentStats.avgRTT = calculateAvgRTT(currentInterfaceName);
   currentStats.avgJitter = calculateAvgJitter(currentInterfaceName);
   currentStats.avgPDR = calculateAvgPDR(currentInterfaceName);
   currentStats.avgThroughput = calculateThroughputOnRx();
   currentStats.lastUpdate = simTime();
   // Update sample counts
   int currSampleCount = 0;
   for (const auto &entry : pingHistory) {
      if (entry.second.pingInterface == currentInterfaceName) currSampleCount++;
   }
   currentStats.sampleCount = currSampleCount;
}

void QoSBasedStrategy::cleanOldEvents() {
   // Cleanup Ping History older than measurement window
   simtime_t currCutOffTime = simTime() - cutOffInterval;    
   auto it = pingHistory.begin();
   while (it != pingHistory.end()) {
      if (it->second.txTime < currCutOffTime) {
         it = pingHistory.erase(it);
      } 
      else {
         ++it;
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
   double score = ( rttScore * weightRTT + pdrScore * weightPDR + 
      jitterScore * weightJitter + throughputScore * weightThroughput 
      ) / totalWeight;

   return score;
}

void QoSBasedStrategy::emitStatistics() {
   manager->emit(currentRTTSignal, currentStats.avgRTT);
   manager->emit(currentJitterSignal, currentStats.avgJitter);
   manager->emit(currentPDRSignal, currentStats.avgPDR);
   manager->emit(currentThroughputSignal, currentStats.avgThroughput);
}

void QoSBasedStrategy::evaluateAndDecide() {
   // 1. Update statistics based on current ping history and traffic stats
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
   double currentScore = computeQoSScore(currentStats);
   manager->emit(qosScoreSignal, currentScore);

   // Skip evaluation if insufficient samples
   if (currentStats.sampleCount < 2) {
      EV_DETAIL << "Insufficient samples (" << currentStats.sampleCount << "), skipping evaluation" << endl;
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
      // trafficStatsRX = {.totalBytes=0.0, .startTime=simTime()};
      // pingHistory.clear(); // cleanOldEvents already cleans old events based on cutOffInterval
   } 
   else {
      consecutiveDegradations = 0;
   }
}







