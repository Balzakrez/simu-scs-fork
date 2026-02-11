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

class EnergyAwareStrategy : public ISwitchingStrategy, public cListener
{
private:
    double criticalEnergyThreshold;
    double lowEnergyThreshold;
    
    // Cost models (energy cost per byte or per second of usage)
    double satelliteEnergyCostPerByte; // Joules per byte
    double cellularEnergyCostPerByte; // Joules per byte
    
    // QoS thresholds
    double maxAcceptableRTT;
    double minUtilityScore;

    // Weights for utility function
    double weightEnergy;
    double weightQos;
    
    // Timers and intervals
    cMessage *evaluationTimer = nullptr; // Timer for periodic evaluation
    simtime_t evaluationInterval; // Interval to evaluate interfaces
    simtime_t lastSwitchTime; // Last time a switch occurred

    simtime_t minHoldTime; // Minimum time to hold an pingInterface before switching again
    simtime_t cutOffInterval; // Cutoff for considering recent measurements in statistics

    // Energy, mobility, and position modules references
    inet::power::IEpEnergyStorage *energyStorage = nullptr;
    inet::IMobility *vehicleMobility = nullptr;
    inet::SatelliteMobilityScs *satMobility = nullptr;
    LUTMotionMobility *gsMobility = nullptr;    
    Satellite::PositionConverter *posConverter = nullptr;
    
    // QoS monitoring
    struct PingEvent {
        simtime_t txTime;
        simtime_t rxTime;
        bool responded;
        std::string pingInterface;
    };
    
    // HistoryMap <PingId, PingEvent> to track ping events
    std::map<long, PingEvent> pingHistory;
    cModule *pingAppModule = nullptr;
    
    // Interface statistics
    struct InterfaceStats {
        double avgRTT;
        double avgPDR;
        double energyConsumed;
        int sampleCount;
    };
    InterfaceStats currentStats;
    std::string currentInterfaceName = "";
    
    // Signals
    simsignal_t currentRTTSignal;
    simsignal_t currentPDRSignal;
    
    simsignal_t residualEnergySignal;
    simsignal_t utilityScoreSignal;

public:
    EnergyAwareStrategy(HybridInterfaceManager *mgr);
    virtual ~EnergyAwareStrategy();
    
    // ISwitchingStrategy pingInterface overrides
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual const char* getStrategyName() const override { return "EnergyAware"; }
    virtual void finish() override;
    
    // cListener pingInterface overrides
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, long value, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
    
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
     * Calculates average RTT for the specified pingInterface.
     * @return The calculated average RTT in seconds.
     */
    double calculateAvgRTT();

    /**
     * Calculates average PDR for the specified pingInterface.
     * @return The calculated average PDR (0.0 to 1.0).
     */
    double calculateAvgPDR();

    /**
     * Updates pingInterface statistics based on recent measurements.
     */
    void updateInterfaceStats();

    /**
     * Cleans up old ping events from the history map to prevent memory bloat.
     */
    void cleanOldPingEvents();
    
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
     * Checks if the current energy level is below the critical threshold.
     * @return True if energy is critical, false otherwise.
     */
    bool isCriticalEnergy();

    /**
     * Checks satellite visibility based on mobility and position information.
     * @return True if the satellite is currently visible, false otherwise.
     */
    bool isSatelliteVisible();
};

#endif
