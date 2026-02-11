// 
// Copyright (C) 2026 Giuseppe Balzano
//
// Note: This strategy is a proof-of-concept for demonstration purposes.
// CoverageBasedStrategy.h - Declaration of coverage-based switching strategy
//

#ifndef __COVERAGEBASEDSTRATEGY_H_
#define __COVERAGEBASEDSTRATEGY_H_

#include "ISwitchingStrategy.h"
#include "../HybridInterfaceManager.h"
#include "os3/mobility/LUTMotionMobility.h"
#include "scs/mobility/SatelliteMobilityScs.h"
#include "scs_utils/converter/PositionConverter.h"


class CoverageBasedStrategy : public ISwitchingStrategy
{
  protected:
    // Parameters
    simtime_t checkInterval;         // Time between coverage checks
    cMessage *checkTimer = nullptr;  // Timer for periodic coverage evaluation
    
    // Pointer to external modules
    inet::IMobility *vehicleMobility = nullptr; // Vehicle mobility
    LUTMotionMobility *gsMobility = nullptr; // Ground station mobility 
    Satellite::PositionConverter *posConverter = nullptr; // Converter (x,y -> lat,lon)

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
