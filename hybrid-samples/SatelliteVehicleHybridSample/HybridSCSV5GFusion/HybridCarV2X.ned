// File: HybridCarV2X.ned
package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

import simu5g.stack.LteNicUe;
import inet.node.inet.StandardHost;

module HybridCarV2X extends StandardHost
{
    parameters:
        @networkNode; // Required by SatelliteNetworkConfiguratorScs
        
        // ========================================
        // Connectivity Configuration
        // ========================================
        bool hasCellular = default(true); // Enable/disable cellular connectivity

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
        int masterId @mutable = default(0); // eNodeB ID to connect to
        int macCellId @mutable = default(0); // Cell ID for LTE
        int macNodeId @mutable = default(0); // MAC node ID
      
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

        // ========================================
        // Statistics & Signals
        // ========================================
        // @signal[packetSent](type=inet::Packet);
        // @signal[packetReceived](type=inet::Packet);
        
        // @statistic[packetReceived](
        //     title="Packets Received"; 
        //     source=packetReceived; 
        //     record=count,"sum(packetBytes)","vector(packetBytes)"; 
        //     interpolationmode=none
        // );
        
        // @statistic[throughput](
        //     title="Throughput"; 
        //     unit=bps; 
        //     source="throughput(packetReceived)"; 
        //     record=vector
        // );
        
        // @statistic[packetSent](
        //     title="Packets Sent"; 
        //     source=packetSent; 
        //     record=count,"sum(packetBytes)","vector(packetBytes)"; 
        //     interpolationmode=none
        // );
        
        // @statistic[rcvdPkLifetime](
        //     title="Received Packet Lifetime"; 
        //     source="dataAge(packetReceived)"; 
        //     unit=s; 
        //     record=stats,vector; 
        //     interpolationmode=none
        // );
        
        // @statistic[endToEndDelay](
        //     title="End-to-End Delay";
        //     source="dataAge(packetReceived)";
        //     unit=s;
        //     record=histogram,vector,stats;
        //     interpolationmode=none
        // );

    gates:
        input lteRadioIn @directIn; // LTE radio input (only used if hasCellular=true)
     
    submodules:
        // ========================================
        // Cellular NIC (conditional, only if hasCellular=true)
        // ========================================
        cellularNic: LteNicUe if hasCellular {
            nodeType = parent.nodeType;
        }
      
    connections allowunconnected:
        // ========================================
        // Cellular Network Layer Connections
        // ========================================
        cellularNic.upperLayerOut --> nl.in++ if hasCellular;
        cellularNic.upperLayerIn <-- nl.out++ if hasCellular;
        lteRadioIn --> cellularNic.radioIn if hasCellular;
}