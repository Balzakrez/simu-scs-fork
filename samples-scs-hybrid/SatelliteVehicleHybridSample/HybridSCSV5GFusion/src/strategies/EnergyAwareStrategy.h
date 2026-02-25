// 
// Copyright (C) 2026 Giuseppe Balzano
//
// EnergyAwareStrategy.h - Energy-aware switching offloading strategy.
//

#ifndef __ENERGYAWARESTRATEGY_H
#define __ENERGYAWARESTRATEGY_H

#include "ISwitchingStrategy.h"
#include "../HybridInterfaceManager.h"
#include "os3/mobility/LUTMotionMobility.h"
#include "scs/mobility/SatelliteMobilityScs.h"
#include "inet/power/contract/IEpEnergyStorage.h"
#include "scs_utils/converter/PositionConverter.h"


using namespace inet::power;

class EnergyAwareStrategy : public ISwitchingStrategy, public cListener {
private:
    double criticalEnergyThreshold;
    
    // Cost models (energy cost per byte or per second of usage)
    double satelliteEnergyCostPerByte; // Joules per byte
    double cellularEnergyCostPerByte; // Joules per byte
    
    // QoS thresholds
    double minAcceptablePDR;
    double maxAcceptableRTT;
    double maxAcceptableJitter;
    // Weights QoS function
    double weightPDR;
    double weightRTT;
    double weightJitter;
    
    // Weights for utility function
    double weightQos;
    double weightEnergy;

    double minUtilityScore;

    
    // Timers and intervals
    cMessage *evaluationTimer = nullptr; // Timer for periodic evaluation
    simtime_t evaluationInterval; // Interval to evaluate interfaces
    simtime_t lastSwitchTime; // Last time a switch occurred
    
    simtime_t minHoldTime; // Minimum time to hold an interface before switching again
    simtime_t cutOffInterval; // Cutoff for considering recent measurements in statistics
    
    // Energy, mobility, and position modules references
    inet::power::IEpEnergyStorage *energyStorage = nullptr;
    inet::IMobility *vehicleMobility = nullptr;
    LUTMotionMobility *gsMobility = nullptr;    
    Satellite::PositionConverter *posConverter = nullptr;
    //inet::SatelliteMobilityScs *satMobility = nullptr;
    
    // Event structure to hold probe information
    struct ProbeEvent {
        simtime_t txTime;
        simtime_t rxTime;
        bool received;
        std::string usedInterface;
    };
    // HistoryMap <SeqNum, ProbeEvent> to track UDP RTT probe events
    std::map<int, ProbeEvent> probeHistory;
    cModule *udpAppModule = nullptr; // Pointer to the UDP application module
    
    // Interface statistics
    struct InterfaceStats {
        double avgRTT;
        double avgPDR;
        double avgJitter;
        int sampleCount;
    };
    InterfaceStats currentInterfaceStatistics;
    
    // TrafficStats structure to hold traffic statistics
    struct TrafficStats {
        double totalBytes = 0.0;
        simtime_t startTime = SIMTIME_ZERO;
    };
    // Throughput tracking on RX
    TrafficStats satelliteRX;
    TrafficStats cellularRX;
    
    // Signals
    simsignal_t currentRTTSignal;
    simsignal_t currentPDRSignal;
    simsignal_t currentJitterSignal;
    
    simsignal_t residualEnergySignal;
    simsignal_t energyEfficiencySignal;
    simsignal_t qosScoreSignal;
    simsignal_t utilityScoreSignal;
    
    std::string currentInterfaceName = "";
    
public:
    EnergyAwareStrategy(HybridInterfaceManager *mgr);
    virtual ~EnergyAwareStrategy();
    
    // ISwitchingStrategy overrides
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual const char* getStrategyName() const override { return "EnergyAware"; }
    virtual void finish() override;
    
    // cListener overrides
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

    virtual void receiveSignal(cComponent *source, simsignal_t signalID, long value, cObject *details) override {};
    
    private:
    
    /**
     * Initializes parameters from the manager module.
     */
    void initializeParameters();
    
    /**
     * Subscribes to relevant signals from the application and mobility modules.
     */
    void subscribeToApplicationSignals();
    
    /**
     * Handles the periodic QoS check timer event.
     */
    void evaluateAndDecide();
    
    /**
     * Calculates average RTT for the specified interface
     * @return The calculated average RTT in seconds.
     */
    double calculateAvgRTT();

    /**
     * Calculates average Jitter for the current interface
     * @return The calculated average Jitter in seconds.
     */
    double calculateAvgJitter();

    /**
     * Calculates average PDR for the specified interface.
     * @return The calculated average PDR (0.0 to 1.0).
     */
    double calculateAvgPDR();

    /**
     * Updates interface statistics based on recent measurements.
     */
    void updateInterfaceStats();

    /**
     * Cleans up old events from the history map to prevent memory bloat.
     */
    void cleanOldEvents();
    
    /**
     * Computes a utility score for the current interface based on energy, cost, and QoS.
     * @return The computed utility score.
     */
    double computeUtilityScore();

    /**
     * Computes the estimated energy cost for using the current interface.
     * @return The estimated energy cost in Joules.
     */
    double computeEnergyEfficiency();

    /**
     * Computes a QoS score based on RTT and PDR.
     * @return The computed QoS score.
     */
    double computeQoSScore();

    /**
     * Checks satellite visibility based on mobility and position information.
     * @return True if the satellite is currently visible, false otherwise.
     */
    bool isSatelliteVisible();

    /**
     * Extracts sequence number from packet name (e.g. "RTT_Probe-42" -> 42).
     * @param pktName The name of the packet from which to extract the sequence number.
     * @return The extracted sequence number, or -1 if extraction fails.
     */
    int extractSeqNumber(const std::string &pktName);
};

#endif
