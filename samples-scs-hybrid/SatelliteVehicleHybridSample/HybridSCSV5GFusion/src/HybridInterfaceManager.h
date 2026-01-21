//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// HybridInterfaceManager.h - Manages hard switching between satellite and cellular interfaces
//

#ifndef __HYBRIDINTERFACEMANAGER_H_
#define __HYBRIDINTERFACEMANAGER_H_

#include <omnetpp.h>
#include "inet/common/InitStages.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "inet/networklayer/ipv4/Ipv4Route.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"

#include "strategies/ISwitchingStrategy.h"
#include "strategies/TimeBasedStrategy.h"
#include "strategies/CoverageBasedStrategy.h"
#include "strategies/EnergyBasedStrategy.h"

using namespace omnetpp;
using namespace inet;

/**
 * HybridInterfaceManager - Manages hard switching between satellite and cellular interfaces.
 * 
 * This simple module implements different switching strategies for HybridCarV2X vehicles.
 * 
 * The network configurator (SatelliteNetworkConfiguratorScs) will detect the carrier
 * state changes and recalculate routes accordingly.
 * 
 */
class HybridInterfaceManager : public cSimpleModule
{
  protected:
    // ========================================
    // Configuration Parameters
    // ========================================
    std::string satInterfaceName; // Name of the satellite interface (e.g., "wlan0")
    std::string cellInterfaceName; // Name of the cellular interface (e.g., "cellular")
    std::string switchingMode; // Type of switching strategy (e.g., "time-based" etc.)
    bool isSatState; // Satellite state: true = satellite active, false = cellular active

    // ========================================
    // Pointers to Network Modules
    // ========================================
    IInterfaceTable *interfaceTable = nullptr; // Interface table for accessing network interfaces
    IIpv4RoutingTable *routingTable = nullptr; // Routing table for route management
    NetworkInterface *satInterface = nullptr; // Pointer to satellite network interface
    NetworkInterface *cellInterface = nullptr; // Pointer to cellular network interface
    
    // ========================================
    // Strategy Pattern for Switching Logic
    // ========================================
    ISwitchingStrategy *strategy = nullptr; // Pointer to switching strategy
    
    // ========================================
    // Signals for statistics and counters
    // ========================================

    // Switching signals
    simsignal_t interfaceSignalId;       
    simsignal_t switchSignalId;            

    // Interface-specific metrics
    simsignal_t satUsageTimeSignalId;           
    simsignal_t cellUsageTimeSignalId;         

    // Counters
    int totalSwitchesCount = 0; // Total number of switches performed
    simtime_t lastSwitchTime;   // Timestamp of the last switch
    simtime_t satTotalTime;     // Total time using satellite interface
    simtime_t cellTotalTime;    // Total time using cellular interface
   
  // **************************************************************************************
  protected:
    /**
     * Returns the number of initialization stages required.
     * Uses INET's NUM_INIT_STAGES for proper multi-stage initialization.
     * @return Number of initialization stages
     */
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    
    /**
     * Multi-stage initialization handler.
     * INITSTAGE_LOCAL: Loads configuration parameters.
     * INITSTAGE_NETWORK_CONFIGURATION: Sets up interfaces and starts timer.
     * @param stage Current initialization stage
     */
    virtual void initialize(int stage) override;
    
    /**
     * Message handler for timer events.
     * Processes the switch timer to toggle interfaces periodically.
     * @param msg Incoming message (expected to be switchTimer)
     */
    virtual void handleMessage(cMessage *msg) override;
    
    /**
     * Create and return the appropriate strategy based on configuration.
     */
    virtual ISwitchingStrategy* createStrategy();

  // **************************************************************************************
  public:
    /**
     * Destructor.
     * Cancels and deletes the switch timer to prevent memory leaks.
     */
    virtual ~HybridInterfaceManager();

    /**
     * Get interface pointers (for strategies that need them).
     */
    NetworkInterface* getSatelliteInterface() const { return satInterface; }
    NetworkInterface* getCellularInterface() const { return cellInterface; }
    NetworkInterface* getCurrentActiveInterface() const { return isSatState ? satInterface : cellInterface; }

    /**
     * Perform interface switch (called by strategies).
     * @param toSatellite true to switch to satellite, false for cellular
     */
    void performSwitch(bool toSatellite);
    
    /**
     * Return current satellite state. Returns true if satellite, false if cellular.
     */
    bool getSatelliteState() const { return isSatState; }

    
    // **************************************************************************************
    protected:

    /**
     * Apply current state to interfaces.
     * Manages satellite carrier state and cellular routes based on isSatState.
     * Note: Hard switching logic.
     */
    virtual void updateInterfaceStates();

    /**
     * Manage cellular default route.
     * Adds the default route through the cellular interface if not existing.
     */
    virtual void ensureCellularDefaultRoute();

    /**
     * Manage satellite default route.
     * Adds the default route through the satellite interface if not existing.
     */
    // virtual void ensureSatelliteDefaultRoute();
   
};

#endif