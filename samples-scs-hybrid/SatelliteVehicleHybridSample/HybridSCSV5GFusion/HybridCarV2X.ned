// File: HybridCarV2X.ned
package SatelliteVehicleHybridSample.HybridSCSV5GFusion;
//package scs_optional.veins_inet_scs;

import simu5g.stack.LteNicUe;
// import simu5g.stack.NRNicUe;
import inet.node.inet.StandardHost;
import SatelliteVehicleHybridSample.HybridSCSV5GFusion.src.HybridInterfaceManager;

module HybridCarV2X extends StandardHost
{
    parameters:
        @networkNode; // Required by SatelliteNetworkConfiguratorScs
        
        // ========================================
        // Connectivity Configuration
        // ========================================
        bool hasCellular = default(true); // Enable/disable cellular connectivity

        // ========================================
        // Hybrid Interface Switching Configuration 
        // ========================================
        bool enableSwitching = default(true); // Enable/disable HybridInterfaceManager
        
        // ========================================
        // Network Layer Configuration
        // ========================================
        forwarding = default(false); // Vehicles are endpoints, not routers
        numWlanInterfaces = default(1); // One interface for satellite communication
        
        // ========================================
        // Simu5G Parameters (only used if hasCellular=true)
        // ========================================
        string extHostAddress = default(""); // External host address (if needed)
        string nodeType = default("UE"); // Node type for Simu5G 

        // 5G NR Parameters (required by Simy5G Binder)
        // int nrMasterId = default(0); // gNodeB ID to connect to (not real parameter)
        // int nrMacCellId = default(0); // Cell ID for 5G connection (not real parameter)
        // int nrMacNodeId @mutable = default(0); // 5G MAC node ID (not real parameter)

        // Legacy 4G/LTE parameters (for backward compatibility required by Simu5G Binder)
        int macNodeId @mutable = default(0); // MAC node ID (not real parameter)
        int masterId @mutable = default(0); // eNodeB ID to connect to (not real parameter)
        int macCellId @mutable = default(0); // Cell ID for LTE connection (not real parameter)

        // ========================================
        // Satellite Interface Configuration (wlan[0])
        // ========================================
        wlan[0].typename = default("AckingWirelessInterface");
        wlan[0].radio.typename = default("ApskScalarRadio");
        // Radio components (compatible with simu-scs satellite models)
        wlan[0].radio.transmitter.typename = default("SatelliteApskScalarTransmitter");
        wlan[0].radio.receiver.typename = default("SatelliteApskScalarReceiver");
        wlan[0].radio.receiver.errorModel.typename = default("SatelliteErrorModel");
        // Antenna configuration (roof-mounted, pointing upward)
        wlan[0].radio.antenna.typename = default("ConstantGainAntenna");
        wlan[0].radio.antenna.mobility.typename = default("AttachedMobility");
        wlan[0].radio.antenna.mobility.mobilityModule = default("^.^.^.^.mobility");
       
        // ========================================
        // Mobility Configuration
        // ========================================
        mobility.typename = default("VeinsInetMobility"); // Integrates with SUMO
        
        // ========================================
        // Routing Configuration
        // ========================================
        *.routingTableModule = default(absPath(".ipv4.routingTable"));
        *.interfaceTableModule = default(absPath(".interfaceTable"));

    gates:
        // Gate for 5G NR radio input (only used if hasCellular=true)
        // input nrRadioIn @directIn; 

        // Legacy 4G/LTE gate (optional, for backward compatibility)
        input lteRadioIn @directIn; // Gate for LTE radio input (only used if hasCellular=true)
     
    submodules:
        // ========================================
        // Hybrid Interface Switching Manager
        // ========================================
        interfaceManager: HybridInterfaceManager if enableSwitching {
            @display("p=100,400");
        }
        
        // ========================================
        // Cellular NIC (conditional, only if hasCellular=true)
        // ========================================
        // cellularNic: NRNicUe if hasCellular {
        //     nodeType = parent.nodeType;
        // }

         cellularNic: LteNicUe if hasCellular {
            nodeType = parent.nodeType;
        }
      
    connections allowunconnected:
        // ========================================
        // Cellular Network Layer Connections
        // ========================================
        cellularNic.upperLayerOut --> nl.in++ if hasCellular;
        cellularNic.upperLayerIn <-- nl.out++ if hasCellular;

        // 5G NR Radio Connections 
        // nrRadioIn --> cellularNic.nrRadioIn if hasCellular;

        // Legacy 4G/LTE Connections (optional, for backward compatibility)
        lteRadioIn --> cellularNic.radioIn if hasCellular;
}