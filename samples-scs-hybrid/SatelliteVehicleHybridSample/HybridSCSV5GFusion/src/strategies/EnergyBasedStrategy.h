// 
//! Copyright (C) 2024 Giusepppe Balzano (check if you need to add more authors)
//
// Implement an energy-aware offloading strategy that dynamically 
// steers traffic between satellite and cellular interfaces. 
// - When battery capacity is high, the system offloads traffic from the terrestrial 
//   cellular network to the satellite link, leveraging its higher throughput. 
// - Conversely, when battery is low, traffic is offloaded back to the 
//   energy-efficient cellular interface, prioritizing device lifetime over link capacity.
//

#ifndef __ENERGYAWARESTRATEGY_H
#define __ENERGYAWARESTRATEGY_H

#include "ISwitchingStrategy.h"
#include "inet/power/contract/IEpEnergyStorage.h"
#include "inet/common/InitStages.h"
#include "scs_utils/converter/PositionConverter.h"
#include "scs/mobility/SatelliteMobilityScs.h"
#include "../HybridInterfaceManager.h"

using namespace inet;
using namespace inet::power; // Namespace for IEpEnergyStorage
using namespace Satellite; // Namespace for PositionConverter

class EnergyBasedStrategy : public ISwitchingStrategy
{
private:
    // Energy thresholds
    double criticalEnergyThreshold;
    double lowEnergyThreshold;
    double highEnergyThreshold;
    std::string satModulePath;       // Path to satellite module
    
    // Energy storage reference
    IEpEnergyStorage* energyStorage = nullptr;
    
    // Timer for periodic energy checks
    cMessage *energyCheckTimer = nullptr;
    simtime_t checkInterval;

    // Pointer to external modules
    IMobility *vehicleMobility = nullptr;        // Vehicle mobility (x,y)
    SatelliteMobilityScs *satMobility = nullptr; // Satellite mobility (orbit calculation)
    PositionConverter *posConverter = nullptr;   // Converter (x,y -> lat,lon)
    
    enum EnergyMode {
        CRITICAL,   // < criticalThreshold → FORCE cellular
        LOW,        // < lowThreshold → PREFER cellular
        MEDIUM,     // < highThreshold → BALANCED
        HIGH        // > highThreshold → CAN USE satellite
    };
    
    // Specific signals and couters for statistics
    simsignal_t residualEnergySignalId;
    
public:
    EnergyBasedStrategy(HybridInterfaceManager *mgr);
    
    virtual ~EnergyBasedStrategy();
    
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual const char* getStrategyName() const override { return "EnergyAware"; }
    
private:
    EnergyMode getCurrentEnergyMode();

    void evaluateAndSwitch();

    bool checkSatelliteVisibility();
};

#endif