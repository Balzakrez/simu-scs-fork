//
// TimeBasedStrategy.h - Time-based switching strategy
//
// Switches interfaces at fixed time intervals (original behavior).
//

#ifndef __TIMEBASEDSTRATEGY_H_
#define __TIMEBASEDSTRATEGY_H_

#include "ISwitchingStrategy.h"
#include "../HybridInterfaceManager.h"
#include "inet/common/InitStages.h"
#include <omnetpp.h>

using namespace omnetpp;
using namespace inet;

/**
 * Time-based switching strategy.
 * 
 * Switches between satellite and cellular at fixed intervals.
 * 
 */
class TimeBasedStrategy : public ISwitchingStrategy
{
  protected:
    simtime_t switchInterval;         // Time between switches
    cMessage *switchTimer = nullptr;  // Timer message for switching

  public:
    TimeBasedStrategy(HybridInterfaceManager *mgr);
    virtual ~TimeBasedStrategy();
    
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual const char* getStrategyName() const override { return "TimeBased"; }
};

#endif