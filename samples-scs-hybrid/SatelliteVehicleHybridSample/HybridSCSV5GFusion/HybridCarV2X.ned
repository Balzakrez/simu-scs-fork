// ==============================================================================
// File: HybridCarV2X.ned
// Hybrid Vehicle Module with Dual Connectivity (Satellite + 5G Cellular)
// ==============================================================================
package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

import simu5g.stack.LteNicUe;
import inet.node.inet.StandardHost;
import SatelliteVehicleHybridSample.HybridSCSV5GFusion.src.HybridInterfaceManager;

// ==============================================================================
// HYBRID VEHICLE MODULE
// Extends StandardHost with dual connectivity capabilities
// ==============================================================================
module HybridCarV2X extends StandardHost
{
    parameters:
        // ======================================================================
        // NETWORK NODE CONFIGURATION
        // ======================================================================
        @networkNode; // Required by SatelliteNetworkConfiguratorScs
        forwarding = default(false); // Vehicles are endpoints, not routers

        // ======================================================================
        // CONNECTIVITY CONFIGURATION
        // ======================================================================
        bool hasCellular = default(true); // Enable/disable cellular connectivity
        numWlanInterfaces = default(1);   // One interface for satellite communication

        // ======================================================================
        // HYBRID INTERFACE SWITCHING CONFIGURATION
        // ======================================================================
        bool enableSwitching = default(true); // Enable/disable HybridInterfaceManager

        // ======================================================================
        // CELLULAR (5G/LTE) CONFIGURATION
        // ======================================================================
        // Simu5G Parameters (only used if hasCellular=true)
        string nodeType = default("UE");
        int macNodeId @mutable = default(0);
        int masterId @mutable = default(0);
        int macCellId @mutable = default(0);
        string extHostAddress = default("");

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
        // MOBILITY CONFIGURATION
        // ======================================================================
        mobility.typename = default("VeinsInetMobility");

        // ======================================================================
        // ROUTING & INTERFACE TABLE MODULES
        // ======================================================================
        *.routingTableModule = default(absPath(".ipv4.routingTable"));
        *.interfaceTableModule = default(absPath(".interfaceTable"));

    // ==========================================================================
    // GATES DEFINITION
    // ==========================================================================
    gates:
        input lteRadioIn @directIn;  // LTE/5G radio input gate

    // ==========================================================================
    // SUBMODULES
    // ==========================================================================
    submodules:
        // Hybrid Interface Manager (Optional)
        interfaceManager: HybridInterfaceManager if enableSwitching {
            @display("p=100,400");
        }
        
        // Cellular Network Interface Card (Optional)
        cellularNic: LteNicUe if hasCellular {
            nodeType = parent.nodeType;
            d2dCapable = false;  // Disable D2D (Device-to-Device)
        }

    // ==========================================================================
    // CONNECTIONS
    // ==========================================================================
    connections allowunconnected:
        // Cellular NIC connections (only if hasCellular=true)
        cellularNic.upperLayerOut --> nl.in++ if hasCellular;
        cellularNic.upperLayerIn <-- nl.out++ if hasCellular;
        lteRadioIn --> cellularNic.radioIn if hasCellular;
}
