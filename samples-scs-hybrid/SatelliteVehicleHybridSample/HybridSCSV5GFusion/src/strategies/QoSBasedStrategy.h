// 
// Copyright (C) 2026 Giuseppe Balzano
//
// QoSBasedStrategy.h - Quality of Service based switching offloading strategy.
//

#ifndef __QOSBASEDSTRATEGY_H
#define __QOSBASEDSTRATEGY_H

#include "ISwitchingStrategy.h"
#include "../HybridInterfaceManager.h"
#include "os3/mobility/LUTMotionMobility.h"
#include "scs/mobility/SatelliteMobilityScs.h"
#include "scs_utils/converter/PositionConverter.h"


class QoSBasedStrategy : public ISwitchingStrategy, public cListener {

protected:

   // Event structure to hold probe information
   struct ProbeEvent {
      simtime_t txTime;
      simtime_t rxTime;
      bool received;
      std::string usedInterface;
   };
   // HistoryMap <SeqNum, ProbeEvent> to track probe events
   std::map<int, ProbeEvent> probeHistory; 
   cModule *udpAppModule = nullptr; // Pointer to the UDP application module

   // InterfaceStats structure to hold computed statistics
   struct InterfaceStats {
      double avgRTT = 0.0;
      double avgJitter = 0.0;
      double avgPDR = 1.0;
      int sampleCount = 0;
      simtime_t lastUpdate = SIMTIME_ZERO;
   };
   InterfaceStats currentInterfaceStatistics;

   // Accumulators for total bytes received
   double satelliteTotalRxBytesAccum = 0.0;
   double satelliteTotalTxBytesAccum = 0.0;
   double cellularTotalRxBytesAccum = 0.0;
   double cellularTotalTxBytesAccum = 0.0;

   // Parameters
   double minAcceptablePDR; // min Packet Delivery Ratio (0.0 - 1.0)
   double maxAcceptableRTT; // max RTT in seconds
   double maxAcceptableJitter; // max jitter in seconds
   double minQosScore; // minimum QoS score to avoid switching
   
   int minDegradationCount; // min number of degraded samples to trigger switch
   double qosCheckInterval; // interval to check QoS in seconds
   
   simtime_t minHoldTime; // minimum time to hold an interface before switching again in seconds
   simtime_t cutOffInterval; // time window for measurements in seconds

   // Weighted scoring
   double weightRTT; // weight for RTT in decision making
   double weightPDR; // weight for PDR in decision making
   double weightJitter; // weight for Jitter in decision making

   /* ************************************************** */
   // Timer and modules
   cMessage *qosCheckTimerMsg = nullptr; // Timer message for periodic QoS checks
   
   // Module references for mobility and position conversion
   inet::IMobility *vehicleMobility = nullptr;
   LUTMotionMobility *gsMobility = nullptr;    
   Satellite::PositionConverter *posConverter = nullptr;
   //inet::SatelliteMobilityScs *satMobility = nullptr;

   // State
   simtime_t lastSwitchTime = SIMTIME_ZERO; // Time of the last interface switch
   std::string currentInterfaceName = "";

   // Counters 
   int consecutiveDegradations = 0;
   
   // Signal Ids for statistics
   simsignal_t currentRTTSignal;
   simsignal_t currentJitterSignal;
   simsignal_t currentPDRSignal;
   simsignal_t degradationCountSignal;
   simsignal_t qosScoreSignal;
  
   /* ************************************************** */

public:
   QoSBasedStrategy(HybridInterfaceManager *mgr);
   virtual ~QoSBasedStrategy();

   // ISwitchingStrategy overrides
   virtual void initialize(int stage) override;
   virtual void finish() override;
   virtual void handleMessage(cMessage *msg) override;
   virtual const char* getStrategyName() const override { return "QoSBased"; }

   // cListener overrides for signal reception
   virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
   
   virtual void receiveSignal(cComponent *source, simsignal_t signalID, long l, cObject *details) override {};
   virtual void receiveSignal(cComponent *source, simsignal_t signalID, double d, cObject *details) override {}; 
   virtual void receiveSignal(cComponent *source, simsignal_t signalID, const char *s, cObject *details) override {};
   virtual void receiveSignal(cComponent *source, simsignal_t signalID, unsigned long l, cObject *details) override {};
   virtual void receiveSignal(cComponent *source, simsignal_t signalID, const SimTime &t, cObject *details) override {};

protected:
   /**
    * Initializes parameters from the manager module.
    */
   void initializeParameters();
   /**
    * Subscribes to necessary application-level signals for monitoring.
    */
   void subscribeToApplicationSignals();

   /**
    * Handles the periodic QoS check timer event.
    */
   void evaluateAndDecide();

   /**
    * Performs the actual interface switch based on the decision.
    */
   void performDecision();
   
   /**
    * Calculates average RTT for the current interface.
    */
   double calculateAvgRTT();
   
   /**
    * Calculates average jitter for the current interface.
    */
   double calculateAvgJitter();

   /**
    * Calculates average Packet Delivery Ratio (PDR) for the current interface.
    */
   double calculateAvgPDR();
   
   /**
    * Updates the statistics for both satellite and cellular interfaces.
    */
   void updateInterfaceStats();
   
   /**
    * Computes a composite QoS score based on weighted metrics.
    * @param stats The InterfaceStats containing the metrics.
    * @return A QoS score where higher is better. 
    * The score is normalized to be between 0 and 1, where 1 represents the best possible QoS and 0 the worst
    */
   double computeQoSScore(const InterfaceStats &stats);

   /**
    * Utility function to determine if the satellite is currently visible based on the mobility modules.
    * @return true if satellite is visible, false otherwise
    */
   bool isSatelliteVisible();
   
   /**
    * Cleans up old events from the history map to prevent memory bloat.
    */
   void cleanOldEvents();

   /**
    * Emit current QoS metrics as statistics for analysis.
    */
   void emitStatistics();

   /**
     * Extracts sequence number from packet name (e.g. "RTT_Probe-42" -> 42).
     * @param pktName The name of the packet from which to extract the sequence number.
     * @return The extracted sequence number, or -1 if extraction fails.
     */
   int extractSeqNumber(const std::string &pktName);

   };

#endif
