package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

//
// HybridInterfaceManager - Manages hard switching between satellite and cellular interfaces.
// 
// This simple module implements time-based interface switching for HybridCarV2X vehicles.
// At each switchInterval, it toggles between satellite (wlan0) and cellular interfaces
// by enabling/disabling the carrier on each interface.
//
// The network configurator (SatelliteNetworkConfiguratorScs) will detect the carrier
// state changes and recalculate routes accordingly.
//
// Key features:
// - Time-based hard switching between two network interfaces
// - Satellite interface managed via carrier state (for routing exclusion)
// - Cellular interface managed via route manipulation (to preserve Simu5G stack)
// - Statistics recording for post-simulation analysis
//
simple HybridInterfaceManager
{
    parameters:
        // Interface Configuration
        string interfaceTableModule = default("^.interfaceTable");
        string satInterfaceName = default("wlan0");      // Satellite interface name
        string cellInterfaceName = default("cellular");  // Cellular interface name (Simu5G)
        
        // Switching Configuration
        double switchInterval @unit(s) = default(60s);   // Time interval between switches
        bool startWithSatellite = default(false);        // If true, starts with Sat ON, Cell OFF
        
        // Statistics signal (1 = satellite active, 0 = cellular active)
        @signal[interfaceSwitch](type=long);
        @statistic[interfaceSwitch](title="Active Interface"; source=interfaceSwitch; record=vector,count; interpolationmode=sample-hold);
}