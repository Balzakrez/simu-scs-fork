package SatelliteVehicleHybridSample.HybridSCSV5GFusion.src.app; 

import inet.applications.contract.IApp;
import inet.applications.udpapp.UdpBasicApp;

simple AlertServerApp extends UdpBasicApp
{
    parameters:
        @display("i=block/app");
        @class(AlertServerApp);
}