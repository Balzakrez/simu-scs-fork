//
// Copyright (C) 2004 OpenSim Ltd.
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
// 
// This program was developed based on INET Framework (https://inet.omnetpp.org/). 
// The original code can be found at `inet/networklayer/configurator/ipv4/L3NetworkConfiguratorBase.h` of INET-4.4.1.

#ifndef INET_SATELLITENETWORKCONFIGURATORSCS_H_
#define INET_SATELLITENETWORKCONFIGURATORSCS_H_

#include "leosatellites/networklayer/configurator/ipv4/SatelliteNetworkConfigurator.h"
#include "inet/common/geometry/common/Coord.h" 

// ===== Forward Declaration =====
namespace Satellite {
    class PositionConverter;
}
// ===============================

namespace inet {

    class INET_API SatelliteNetworkConfiguratorScs : public SatelliteNetworkConfigurator {
        
        protected:
            Satellite::PositionConverter* posConverter = nullptr;
        
        private:
            /**
             * Validates Veins vehicle position coordinates.
             * Checks for NaN, infinity, and reasonable coordinate ranges.
             * @param pos Position coordinates to validate
             * @return true if position is valid, false otherwise
             */
             bool isValidVeinsPosition(const inet::Coord& pos);

            /**
             * Checks if a module is a Veins node.
             * @param mod Module to check
             * @return true if module is a Veins node, false otherwise
             */
            bool isVeinsNode(cModule* mod);
            /**
             * Determines if a route is protected (should not be deleted).
             * Protected routes include cellular and LTE interfaces.
             * @param route Route to check
             * @return true if route is protected, false otherwise
             */
            bool isProtectedRoute(Ipv4Route* route);

            /**
             * Validates IP configuration for Veins nodes.
             * Ensures Veins nodes have valid IP addresses after configuration.
             */
            void validateVeinsNodeIp();
            

            /**
             * Determines if a link should be excluded from routing.
             * Excludes links with interfaces that are down or have no carrier.
             * @param link Network link to evaluate
             * @return true if link should be excluded, false otherwise
             */
            bool isToExcludeLink(Link *link);

            /**
             * Prints the current routing table for debugging.
             */
            void printRoutingTable();

        protected:

            /**
             * Override initialize to get parameters from ned/ini file.
             * Caches the PositionConverter module for coordinate conversions.
             * @param stage Initialization stage number
             */
            virtual void initialize(int stage) override;

            /**
             * Override handleMessage to use reinvokeConfigurator.
             * Handles periodic reconfiguration timer messages.
             * @param msg Message to handle
             */
            virtual void handleMessage(cMessage *msg) override;
        
            /**
             * Function to reinvoke the configurator during simulation to assign routes and IP addresses
             * for dynamic topologies with dynamic vehicles such as SUMO/Veins vehicles.
             * Handles cleanup, topology re-extraction, and routing recalculation.
             * @param topology Network topology to reconfigure
             * @param autorouteElement XML element containing autoroute configuration
             */
            virtual void reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement) override;
       
            /**
             * Computes wireless link weight (cost) for routing algorithms.
             * Handles satellite-to-vehicle, vehicle-to-satellite, and ground station links.
             * Uses PositionConverter for coordinate transformations.
             * @param link Network link to evaluate
             * @param metric Metric type (hopCount, propagationDelay, etc.)
             * @param parameters XML parameters for cost calculation
             * @return Computed link weight/cost
             */
            virtual double computeWirelessLinkWeight(Link *link, const char *metric, cXMLElement *parameters) override;
            
            /**
             * Computes wired link weight (cost) for routing algorithms.
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
