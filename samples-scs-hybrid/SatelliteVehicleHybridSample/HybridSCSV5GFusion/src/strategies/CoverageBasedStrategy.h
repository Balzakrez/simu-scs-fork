//
// CoverageBasedStrategy.h - Declaration of coverage-based switching strategy
//

#ifndef __COVERAGEBASEDSTRATEGY_H_
#define __COVERAGEBASEDSTRATEGY_H_

#include "ISwitchingStrategy.h"
#include "../HybridInterfaceManager.h"
#include "inet/mobility/contract/IMobility.h"

// Custom specific headers
#include "scs_utils/converter/PositionConverter.h"
#include "scs/mobility/SatelliteMobilityScs.h"

using namespace inet;
using namespace Satellite; // Namespace for PositionConverter

class CoverageBasedStrategy : public ISwitchingStrategy
{
  protected:
    // Parameters
    simtime_t checkInterval;         // Time between coverage checks
    std::string satModulePath;       // Path to satellite module
    cMessage *checkTimer = nullptr;  // Timer for periodic coverage evaluation
    
    // Pointer to external modules
    IMobility *vehicleMobility = nullptr;         // Vehicle mobility (x,y)
    SatelliteMobilityScs *satMobility = nullptr;  // Satellite mobility (orbit calculation)
    PositionConverter *posConverter = nullptr;    // Converter (x,y -> lat,lon)

    // Specific signals and counters for statistics
    simsignal_t elevationSignalId;          // Elevation angle signal 
    simsignal_t coverageLossCountSignalId;  // Coverage loss count signal
    int coverageLossCount = 0;              // Count of coverage loss events


  public:
    CoverageBasedStrategy(HybridInterfaceManager *mgr);
    virtual ~CoverageBasedStrategy();
    
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual const char* getStrategyName() const override { return "CoverageBased"; }
    
  protected:
    /**
     * Evaluates current coverage conditions and decides on interface switching.
     * Checks satellite visibility and elevation, and cellular availability.
     */
    void evaluateCoverage();
    
    /**
     * Calculates if the satellite is geometrically visible.
     * Uses PositionConverter to transform the vehicle's position
     * and queries the SatelliteMobility module for the current elevation.
     */
    bool isSatelliteVisible(); 
    
    /**
     * Determines if cellular interface is available.
     */
    bool isCellularAvailable();
};

#endif
