//
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
// Copyright (C) 2026 Giuseppe Balzano
//
// SPDX-License-Identifier: LGPL-3.0-or-later
// 
// This program was developed based on INET Framework (https://inet.omnetpp.org/). 
// The original code can be found at `inet/networklayer/configurator/ipv4/L3NetworkConfiguratorBase.h` of INET-4.4.1.

#ifndef INET_SATELLITENETWORKCONFIGURATORSCS_H_
#define INET_SATELLITENETWORKCONFIGURATORSCS_H_

#include "leosatellites/networklayer/configurator/ipv4/SatelliteNetworkConfigurator.h"
#include "inet/common/geometry/common/Coord.h"
#include "common/binder/Binder.h" // from Simu5G
#include "veins_inet_scs/VeinsInetMobility.h" // from scs_optional

#include <queue>

// ===== Forward Declaration =====
namespace Satellite {
    class PositionConverter;
}
// ===============================

namespace inet {

class INET_API SatelliteNetworkConfiguratorScs : public SatelliteNetworkConfigurator {
    
public:

    SatelliteNetworkConfiguratorScs() :  wlanPoolPtr(nullptr), cellularPoolPtr(nullptr), posConverterPtr(nullptr) {}

    virtual ~SatelliteNetworkConfiguratorScs();

protected:

    // Cached PositionConverter for veins nodes coordinate conversions
    Satellite::PositionConverter* posConverterPtr = nullptr;

    /* ************************************************************ */
    /** IP Pool Management for Vehicle Interfaces 
     * @param usedAddressesSet Set of currently allocated IP addresses
     * @param freeAddressesQueue Queue of released IP addresses available for reuse
     * @param baseAddress Base network address
     * @param nextAddress Next host address to allocate
     * @param netmask Network mask
     * @param maxHosts Maximum number of hosts in the subnet
     * **/
    struct IpPool {
        std::set<uint32_t> usedAddressesSet;    // Set of currently allocated IP addresses
        std::queue<uint32_t> freeAddressesQueue; // Queue of released IP addresses available for reuse
        uint32_t baseAddress = 0;
        uint32_t nextAddress = 0;
        uint32_t netmask = 0;
        uint32_t maxHosts = 0;
        // Constructor to initialize the IP pool with base address and netmask
        IpPool(const Ipv4Address& baseAddress, const Ipv4Address& netmask) {
            this->baseAddress = baseAddress.getInt();
            this->nextAddress = 100; // Start from 100 (skip network .0 address and .1 gateway address)
            this->netmask = netmask.getInt();
            this->maxHosts = (~netmask.getInt()) - 1; // Exclude broadcast address
        }
    };
    
    // IP Pools for vehicle interfaces (wlan0 and cellular)
    IpPool* wlanPoolPtr = nullptr;
    IpPool* cellularPoolPtr = nullptr;

    // Maps to track allocated IPs for nodes
    std::map<int, uint32_t> nodeToWlanIpMap; 
    std::map<int, uint32_t> nodeToCellularIpMap;
    /* ************************************************************ */
    // Map of nodeIds to MacNodeIds for Binder registration/un-registration
    std::map<int, unsigned short> nodeIdToMacNodeIdMap;
    
private:
    /**
     * @brief Validates Veins vehicle position coordinates.
     * Checks for NaN, infinity, and reasonable coordinate ranges.
     * @param pos Position coordinates to validate
     * @return true if position is valid, false otherwise
     */
    virtual bool isValidVeinsPosition(const inet::Coord& pos);

    /**
     * @brief Checks if a module is a Veins node.
     * @param mod Module to check
     * @return true if module is a Veins node, false otherwise
     */
    virtual bool isVeinsNode(cModule* mod);

    /**
     * @brief Configures vehicle interfaces with IP addresses from pools.
     * The address is assigned from the appropriate pool (wlan or cellular).
     * Add direct routes for the assigned interfaces if not existing.
     * Add default routes for the assigned interfaces if not existing.
     */
    virtual void checkAndConfigureVehicleInterfaces();
    
    /**
     * @brief Determines if a link should be excluded from routing.
     * Excludes links with interfaces that are down or have no carrier.
     * @param link Network link to evaluate
     * @return true if link should be excluded, false otherwise
     */
    virtual bool isToExcludeLink(Link *link);

    /**
     * @brief Optimizes routes for Veins vehicles.
     * Removes unnecessary routes and ensures connectivity using current active interfaces.
     * Cleans up routes associated with down interfaces.
     */
    virtual void optimizeVehiclesRoutes();

    /**
     * @brief Filters satellite links for Veins vehicles based on satellite reachability.
     * Disables links where the satellite elevation is below a defined threshold.
     * The default threshold is 25 degrees (leosatellites/NoradA).
     * @param topology Network topology to process
     */
    virtual void filterVeinsSatelliteLinks(Topology& topology);

    /**
     * @brief Prints the current routing table for debugging.
     */
    virtual void printRoutingTable();

    /* ************************************************************ */

    /** @brief Allocation and release of IP addresses from pools 
     *  @param pool Pointer to the IP pool
     *  @param moduleId Module ID of the node requesting the IP
     *  @param nodeToIpMap Map of module IDs to allocated IP addresses 
     */
    virtual Ipv4Address allocateIpFromPool(IpPool* pool, int moduleId, std::map<int, uint32_t>& nodeToIpMap);

    /** @brief Release of IP addresses from pools 
     *  @param pool Pointer to the IP pool
     *  @param moduleId Module ID of the node releasing the IP
     *  @param nodeToIpMap Map of module IDs to allocated IP addresses
     */
    virtual void releaseIpToPool(IpPool* pool, int moduleId, std::map<int, uint32_t>& nodeToIpMap);
    
    /** @brief Dump IP Pools */
    virtual void dumpIpPools();

    /* ************************************************************ */

    /** @brief Binder Registration 
     *  @param newAddr New IP address to register
     *  @param host Module to register the IP address to
     */
    virtual void registerToBinder(const Ipv4Address newAddr, cModule* host);

    /** @brief Binder Registration and Unregistration 
     *  @param moduleId Module ID of the node to unregister
     */
    virtual void unRegisterFromBinder(int moduleId);

    /**
     *  @brief Find MacNodeId for a node using multiple fallback methods
     *  @param host Module host
     *  @param cellularIp Cellular IP address (optional, for IP-based lookup)
     *  @return MacNodeId if found, 0 otherwise
     */
    virtual MacNodeId findMacNodeId(cModule* host, const Ipv4Address& cellularIp = Ipv4Address::UNSPECIFIED_ADDRESS);

    /* ************************************************************ */
    
    /**
     * @brief Cleans up exited Veins nodes from IP pools and binder.
     */
    virtual void cleanupVeinsNode();

protected:

    /**
     * @brief Override initialize to get parameters from ned/ini file.
     * Caches the PositionConverter module for coordinate conversions.
     * @param stage Initialization stage number
     */
    virtual void initialize(int stage) override;

    /**
     * @brief Override handleMessage to use reinvokeConfigurator.
     * Handles periodic reconfiguration timer messages.
     * @param msg Message to handle
     */
    virtual void handleMessage(cMessage *msg) override;

    /**
     * @brief Function to reinvoke the configurator during simulation to assign routes and IP addresses
     * for dynamic topologies with dynamic vehicles such as SUMO/Veins vehicles.
     * Handles cleanup, topology re-extraction, and routing recalculation.
     * @param topology Network topology to reconfigure
     * @param autorouteElement XML element containing autoroute configuration
     */
    virtual void reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement) override;


    /**
     * @brief Calculate propagation delay from distance
     * @param distanceKm Distance in kilometers
     * @return Delay in seconds, or INFINITY if invalid
    */
    virtual double calculatePropagationDelay(double distanceKm);

    /**
     * @brief Get vehicle geographic coordinates from Veins position
     * @param veinsMob Veins mobility module
     * @param outLat Output latitude
     * @param outLon Output longitude
     * @return true if successful, false if position invalid
     */
    virtual bool checkAndGetVehicleGeoCoordinates(veins::VeinsInetMobility* veinsMob, double& outLat, double& outLon);

    /**
     * @brief Computes wireless link weight (cost) for routing algorithms.
     * Handles satellite-to-vehicle, vehicle-to-satellite, and ground station links.
     * Uses PositionConverter for coordinate transformations.
     * @param link Network link to evaluate
     * @param metric Metric type (hopCount, propagationDelay, etc.)
     * @param parameters XML parameters for cost calculation
     * @return Computed link weight/cost
     */
    virtual double computeWirelessLinkWeight(Link *link, const char *metric, cXMLElement *parameters) override;
    
    /**
     * @brief Computes wired link weight (cost) for routing algorithms.
     * Handles delay, dataRate, errorRate, and hopCount metrics.
     * @param link Network link to evaluate
     * @param metric Metric type (hopCount, delay, dataRate, errorRate)
     * @param parameters XML parameters for cost calculation
     * @return Computed link weight/cost
     */
    virtual double computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters) override;
};

} // namespace inet

#endif /* INET_SATELLITENETWORKCONFIGURATORSCS_H_ */
