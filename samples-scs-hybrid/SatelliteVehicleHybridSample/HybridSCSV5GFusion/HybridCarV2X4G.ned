// ==============================================================================
// File: HybridCarV2X4G.ned
// Hybrid Vehicle Module with Dual Connectivity (Satellite + 4G LTE)
// Extends Ue to provide native LTE support with satellite interface
// ==============================================================================
package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

import simu5g.nodes.Ue;
import inet.linklayer.contract.IWirelessInterface;
import SatelliteVehicleHybridSample.HybridSCSV5GFusion.src.HybridInterfaceManager;


import inet.common.lifecycle.NodeStatus;
import inet.power.contract.IEpEnergyStorage;
import inet.power.contract.IEpEnergyManagement;

// ==============================================================================
// HYBRID VEHICLE MODULE WITH 4G LTE
// Extends Ue with satellite connectivity capabilities
// ==============================================================================
module HybridCarV2X4G extends Ue
{
    parameters:
        // ======================================================================
        // NETWORK NODE CONFIGURATION
        // ======================================================================
        @display("i=device/car");

        // ======================================================================
        // CONNECTIVITY CONFIGURATION
        // ======================================================================
        int numWlanInterfaces = default(1);

        // ======================================================================
        // HYBRID INTERFACE SWITCHING CONFIGURATION
        // ======================================================================
        bool enableSwitching = default(true);

        // ======================================================================
        // SATELLITE INTERFACE CONFIGURATION
        // ======================================================================
        wlan[0].typename = default("WirelessInterface");

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
        wlan[numWlanInterfaces]: <default("WirelessInterface")> like IWirelessInterface {
            @display("p=200,589;q=queue");
        }
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
        for i=0..numWlanInterfaces-1 {
            wlan[i].upperLayerOut --> nl.in++;
            wlan[i].upperLayerIn <-- nl.out++;
        }
}


// // ==============================================================================
// // File: HybridCarV2X.ned
// // Hybrid Vehicle Module with Dual Connectivity (Satellite + 4G LTE Cellular)
// // ==============================================================================
// package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

// import simu5g.stack.LteNicUe;
// import inet.node.inet.StandardHost;
// import SatelliteVehicleHybridSample.HybridSCSV5GFusion.src.HybridInterfaceManager;

// // ==============================================================================
// // HYBRID VEHICLE MODULE
// // Extends StandardHost with dual connectivity capabilities
// // ==============================================================================
// module HybridCarV2X4G extends StandardHost
// {
//     parameters:
//         // ======================================================================
//         // NETWORK NODE CONFIGURATION
//         // ======================================================================
//         @networkNode; // Required by SatelliteNetworkConfiguratorScs
//         forwarding = default(false); // Vehicles are endpoints, not routers

//         // ======================================================================
//         // CONNECTIVITY CONFIGURATION
//         // ======================================================================
//         bool hasCellular = default(true); // Enable/disable cellular connectivity
//         numWlanInterfaces = default(1);   // One interface for satellite communication

//         // ======================================================================
//         // HYBRID INTERFACE SWITCHING CONFIGURATION
//         // ======================================================================
//         bool enableSwitching = default(true); // Enable/disable HybridInterfaceManager

//         // ======================================================================
//         // CELLULAR (5G/LTE) CONFIGURATION
//         // ======================================================================
//         // Simu5G Parameters (only used if hasCellular=true)
//         string nodeType = default("UE");
//         int macNodeId @mutable = default(0);
//         int masterId @mutable = default(0);
//         int macCellId @mutable = default(0);
//         string extHostAddress = default("");

//         // ======================================================================
//         // SATELLITE INTERFACE CONFIGURATION
//         // ======================================================================
//         // Interface Type
//         wlan[0].typename = default("WirelessInterface");
//         // wlan[0].typename = default("AckingWirelessInterface");

//         // Physical Layer (Radio)
//         wlan[0].radio.typename = default("ApskScalarRadio");
//         wlan[0].radio.transmitter.typename = default("SatelliteApskScalarTransmitter");
//         wlan[0].radio.receiver.typename = default("SatelliteApskScalarReceiver");
//         wlan[0].radio.receiver.errorModel.typename = default("SatelliteErrorModel");
//         wlan[0].radio.antenna.typename = default("ConstantGainAntenna");
//         wlan[0].radio.antenna.mobility.typename = default("AttachedMobility");
//         wlan[0].radio.antenna.mobility.mobilityModule = default("^.^.^.^.mobility");

//         // MAC Layer & Queue
//         wlan[0].mac.typename = default("CsmaCaMac");
//         wlan[0].queue.typename = default("DropTailQueue");

//         // ======================================================================
//         // MOBILITY CONFIGURATION
//         // ======================================================================
//         mobility.typename = default("VeinsInetMobility");

//         // ======================================================================
//         // ROUTING & INTERFACE TABLE MODULES
//         // ======================================================================
//         *.routingTableModule = default(absPath(".ipv4.routingTable"));
//         *.interfaceTableModule = default(absPath(".interfaceTable"));

//     // ==========================================================================
//     // GATES DEFINITION
//     // ==========================================================================
//     gates:
//         input lteRadioIn @directIn;  // LTE/5G radio input gate

//     // ==========================================================================
//     // SUBMODULES
//     // ==========================================================================
//     submodules:
//         // Hybrid Interface Manager (Optional)
//         interfaceManager: HybridInterfaceManager if enableSwitching {
//             @display("p=100,400");
//         }
        
//         // Cellular Network Interface Card (Optional)
//         cellularNic: LteNicUe if hasCellular {
//             nodeType = parent.nodeType;
//             d2dCapable = false;  // Disable D2D (Device-to-Device)
//         }

//     // ==========================================================================
//     // CONNECTIONS
//     // ==========================================================================
//     connections allowunconnected:
//         // Cellular NIC connections (only if hasCellular=true)
//         cellularNic.upperLayerOut --> nl.in++ if hasCellular;
//         cellularNic.upperLayerIn <-- nl.out++ if hasCellular;
//         lteRadioIn --> cellularNic.radioIn if hasCellular;
// }
