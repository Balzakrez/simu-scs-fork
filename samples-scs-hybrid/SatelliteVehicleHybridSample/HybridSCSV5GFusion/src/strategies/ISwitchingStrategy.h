//
// ISwitchingStrategy.h - Base interface for switching strategies
//

#ifndef __ISWITCHINGSTRATEGY_H_
#define __ISWITCHINGSTRATEGY_H_

#include <omnetpp.h>
#include "inet/networklayer/common/NetworkInterface.h"

using namespace inet;
using namespace omnetpp;

// Forward declaration
class HybridInterfaceManager;

/**
 * Abstract base class for interface switching strategies.
 * 
 * Concrete strategies implement the decision logic for when to switch
 * between satellite and cellular interfaces.
 */
class ISwitchingStrategy
{
  protected:
    HybridInterfaceManager *manager = nullptr;  // Pointer to manager module
    
  public:
    ISwitchingStrategy(HybridInterfaceManager *mgr) : manager(mgr) {}
    
    /**
     * Virtual destructor.
     * Ensures proper cleanup in derived classes.
     */
    virtual ~ISwitchingStrategy() {}
    
    /**
     * Initialize the strategy.
     * Called during manager initialization (INITSTAGE_APPLICATION).
     * @param stage Initialization stage
     */
    virtual void initialize(int stage) = 0;
    
    /**
     * Handle incoming messages (timers, signals, etc).
     * @param msg Message to handle
     */
    virtual void handleMessage(cMessage *msg) = 0;

    /**
     * Get strategy name for logging.
     */
    virtual const char* getStrategyName() const = 0;



};

#endif
