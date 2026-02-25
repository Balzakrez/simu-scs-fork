// ==============================================================================
// File: HybridCarV2X5G.ned
// Hybrid Vehicle Module with Dual Connectivity (Satellite + 5G NR)
// Extends NRUe to provide native 5G NR support with satellite interface
// ==============================================================================
package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

import simu5g.stack.NRNicUe;
import simu5g.nodes.NR.NRUe;
import inet.linklayer.contract.IWirelessInterface;
import SatelliteVehicleHybridSample.HybridSCSV5GFusion.src.HybridInterfaceManager;

import inet.common.lifecycle.NodeStatus;
import inet.power.contract.IEpEnergyStorage;
import inet.power.contract.IEpEnergyManagement;

// ==============================================================================
// HYBRID VEHICLE MODULE WITH 5G NR
// Extends NRUe with satellite connectivity capabilities
// ==============================================================================
module HybridCarV2X5G extends NRUe
{
    parameters:
        // ======================================================================
        // NETWORK NODE CONFIGURATION
        // ======================================================================
        @display("i=device/car");

        // ======================================================================
        // CONNECTIVITY CONFIGURATION
        // ======================================================================
        int numWlanInterfaces = default(1); // One interface for satellite communication

        // ======================================================================
        // HYBRID INTERFACE SWITCHING CONFIGURATION
        // ======================================================================
        bool enableSwitching = default(true); // Enable/disable HybridInterfaceManager

        // ======================================================================
        // SATELLITE INTERFACE CONFIGURATION
        // ======================================================================
        // Interface Type
        wlan[0].typename = default("WirelessInterface");
        // wlan[0].typename = default("AckingWirelessInterface");

        // Physical Layer (Radio)
        wlan[0].radio.typename = default("ApskScalarRadio");
        wlan[0].radio.transmitter.typename = default("SatelliteApskScalarTransmitter");
        wlan[0].radio.receiver.typename = default("SatelliteApskScalarReceiver");
        wlan[0].radio.receiver.errorModel.typename = default("SatelliteErrorModel");
        wlan[0].radio.antenna.typename = default("ConstantGainAntenna");
        wlan[0].radio.antenna.mobility.typename = default("AttachedMobility");
        wlan[0].radio.antenna.mobility.mobilityModule = default("^.^.^.^.mobility");

        // MAC Layer & Queue
        wlan[0].mac.typename = default("CsmaCaMac");
        wlan[0].queue.typename = default("DropTailQueue");

        // ======================================================================
        // ROUTING & INTERFACE TABLE MODULES
        // ======================================================================
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        *.routingTableModule = default(absPath(".ipv4.routingTable"));
        *.energySourceModule = default(absPath(".energyStorage"));

    // ==========================================================================
    // SUBMODULES
    // ==========================================================================
    submodules:
        // Satellite Network Interface
        wlan[numWlanInterfaces]: <default("WirelessInterface")> like IWirelessInterface {
            @display("p=200,589;q=queue");
        }
        // Hybrid Interface Manager
        interfaceManager: HybridInterfaceManager if enableSwitching {
            @display("p=100,400");
        }
        status: NodeStatus {
            @display("p=100,200;is=s");
        }
        energyStorage: <default("SimpleEpEnergyStorage")> like inet.power.contract.IEpEnergyStorage {
            @display("p=100,500;i=block/plug");
        }
        energyManagement: <default("SimpleEpEnergyManagement")> like IEpEnergyManagement {
            @display("p=100,600;i=block/plug");
        }
    // ==========================================================================
    // CONNECTIONS
    // ==========================================================================
    connections allowunconnected:
        // NRUe already connects nrRadioIn <-> cellularNic <-> nl internally
        
        // Connect satellite interface to network layer
        for i=0..numWlanInterfaces-1 {
            wlan[i].upperLayerOut --> nl.in++;
            wlan[i].upperLayerIn <-- nl.out++;
        }
}