// 
// Copyright (C) 2026 Giuseppe Balzano
//
// EnergyAwareStrategy.h - Energy-aware switching offloading strategy.
//

#ifndef __ENERGYAWARESTRATEGY_H
#define __ENERGYAWARESTRATEGY_H

#include "ISwitchingStrategy.h"
#include "inet/power/contract/IEpEnergyStorage.h"
#include "inet/common/InitStages.h"
#include "scs_utils/converter/PositionConverter.h"
#include "scs/mobility/SatelliteMobilityScs.h"
#include "../HybridInterfaceManager.h"
#include <deque>
#include <map>

using namespace inet;
using namespace inet::power;
using namespace Satellite;

class EnergyAwareStrategy : public ISwitchingStrategy, public cListener
{
protected:
    enum OptimizationMode {
        MINIMIZE_ENERGY, // Prioritize energy efficiency
        MAXIMIZE_QOS,    // Prioritize performance
        BALANCED         // Balance energy, cost, and QoS
    };

private:
    // Configuration
    OptimizationMode optimizationMode;
    double criticalEnergyThreshold;
    double lowEnergyThreshold;
    
    // Cost models (energy cost per byte or per second of usage)
    double satelliteEnergyCostPerByte; // Joules per byte
    double cellularEnergyCostPerByte; // Joules per byte
    
    // QoS thresholds
    double minAcceptableRTT;
    double maxAcceptableRTT;
    double minAcceptablePDR;
    
    // Timers and intervals
    cMessage *evaluationTimer = nullptr; // Timer for periodic evaluation
    simtime_t evaluationInterval; // Interval to evaluate interfaces
    simtime_t minHoldTime; // Minimum time to hold an interface before switching again
    simtime_t lastSwitchTime; // Last time a switch occurred
    
    // Energy and mobility
    IEpEnergyStorage *energyStorage = nullptr;
    IMobility *vehicleMobility = nullptr;
    SatelliteMobilityScs *satMobility = nullptr;
    PositionConverter *posConverter = nullptr;
    std::string satModulePath;
    
    // QoS monitoring
    struct PingEvent {
        simtime_t txTime;
        simtime_t rxTime;
        bool responded;
        std::string interface;
    };
    
    // HistoryMap <PingId, PingEvent> to track ping events
    std::map<long, PingEvent> pingHistory;
    cModule *pingAppModule = nullptr;
    
    std::string currentInterface;
    
    // Interface statistics
    struct InterfaceStats {
        double avgRTT;
        double avgPDR;
        double energyConsumed;
        int sampleCount;
    };
    InterfaceStats satelliteStats;
    InterfaceStats cellularStats;
    
    // Signals
    simsignal_t currentRTTSignal;
    simsignal_t currentPDRSignal;
    
    simsignal_t residualEnergySignal;
    simsignal_t utilityScoreSignal;

public:
    EnergyAwareStrategy(HybridInterfaceManager *mgr);
    virtual ~EnergyAwareStrategy();
    
    // ISwitchingStrategy interface overrides
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual const char* getStrategyName() const override { return "EnergyAware"; }
    virtual void finish() override;
    
    // cListener interface overrides
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
     * Calculates average RTT for the specified interface.
     * @return The calculated average RTT in seconds.
     */
    double calculateAvgRTT();

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
     * Cleans up old ping events from the history map to prevent memory bloat.
     */
    void cleanOldPingEvents();
    
    /**
     * Computes a utility score for the specified interface based on energy, cost, and QoS.
     * @return The computed utility score.
     */
    double computeUtilityScore(const std::string &interface);

    /**
     * Computes the estimated energy cost for using the specified interface.
     * @return The estimated energy cost in Joules.
     */
    double computeEnergyCost(const std::string &interface);

    /**
     * Computes a QoS score for the specified interface based on RTT and PDR.
     * @return The computed QoS score.
     */
    double computeQoSScore(const std::string &interface);

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
    
    /**
      * Determines the best interface to use based on the computed utility scores.
      * @return The name of the best interface ("satellite" or "cellular").
      */
    std::string getBestInterface();
};

#endif