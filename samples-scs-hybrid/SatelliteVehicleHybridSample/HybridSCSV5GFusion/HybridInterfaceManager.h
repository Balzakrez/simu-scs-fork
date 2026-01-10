//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// HybridInterfaceManager.h - Manages hard switching between satellite and cellular interfaces
//
// This module implements time-based interface switching for HybridCarV2X vehicles.
// At each switchInterval, it toggles between satellite (wlan0) and cellular interfaces
// by managing carrier states and routing tables accordingly.
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

using namespace omnetpp;
using namespace inet;

/**
 * HybridInterfaceManager - Manages hard switching between satellite and cellular interfaces.
 * 
 * This simple module implements time-based interface switching for HybridCarV2X vehicles.
 * At each switchInterval, it toggles between satellite (wlan0) and cellular interfaces
 * by enabling/disabling the carrier on each interface.
 * 
 * The network configurator (SatelliteNetworkConfiguratorScs) will detect the carrier
 * state changes and recalculate routes accordingly.
 */
class HybridInterfaceManager : public cSimpleModule
{
  protected:
    // ========================================
    // Configuration Parameters
    // ========================================
    simtime_t switchInterval;       // Time interval between interface switches
    std::string satInterfaceName;   // Name of the satellite interface (e.g., "wlan0")
    std::string cellInterfaceName;  // Name of the cellular interface (e.g., "cellular")
    bool isSatActive;               // Current state: true = satellite active, false = cellular active

    // ========================================
    // Pointers to Network Modules
    // ========================================
    IInterfaceTable *ptrInterfaceTable = nullptr;   // Interface table for accessing network interfaces
    IIpv4RoutingTable *ptrRoutingTable = nullptr;   // Routing table for route management
    NetworkInterface *ptrSatInterface = nullptr;    // Pointer to satellite network interface
    NetworkInterface *ptrCellInterface = nullptr;   // Pointer to cellular network interface
    
    // ========================================
    // Timer Management
    // ========================================
    cMessage *switchTimer = nullptr;  // Self-message for periodic interface switching

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
     * Switch interface states (called by the timer).
     * Inverts isSatActive and updates interface states accordingly.
     * Emits statistics signal for post-simulation analysis.
     */
    virtual void switchInterfaces();
    
    /**
     * Apply current state to interfaces.
     * Manages satellite carrier state and cellular routes based on isSatActive.
     * Note: Does not modify cellular carrier to avoid breaking Simu5G stack.
     */
    virtual void updateInterfaceStates();
    
    /**
     * Manage cellular default route.
     * Adds or removes the default route through the cellular interface.
     * @param enable true to add default route if not existing, false to remove it
     */
    virtual void updateCellularDefaultRoute(bool enable);
    
    /**
     * Remove all routes using satellite interface.
     * Called when switching to CELLULAR mode to cleanup stale routes.
     */
    virtual void cleanupSatelliteRoutes();
    
    /**
     * Remove specific cellular routes.
     * Called when switching to SATELLITE mode to cleanup non-default routes.
     * Preserves the default route which is handled separately.
     */
    virtual void cleanupCellularRoutes();

  public:
    /**
     * Destructor.
     * Cancels and deletes the switch timer to prevent memory leaks.
     */
    virtual ~HybridInterfaceManager();
};

#endif