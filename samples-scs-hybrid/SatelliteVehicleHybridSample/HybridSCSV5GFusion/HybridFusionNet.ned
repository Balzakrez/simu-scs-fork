//
// File: SV5GFusion4.ned - Network definition for Hybrid Satellite-Vehicle 5G Fusion Sample
// 

package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

// ==============================================================================
// CUSTOM IMPORTS
// ==============================================================================
import SatelliteVehicleHybridSample.HybridSCSV5GFusion.HybridCarV2X;

// ==============================================================================
// SCS IMPORTS
// ==============================================================================
import scs.base.GroundStation;
import scs.base.LaserSatellite;
import scs.common.binder.SBinder;

// ==============================================================================
// INET IMPORTS
// ==============================================================================
import inet.node.inet.Router;
import inet.node.ethernet.Eth10G;
import inet.node.inet.StandardHost;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.visualizer.contract.IIntegratedVisualizer;
import inet.physicallayer.wireless.apsk.packetlevel.ApskScalarRadioMedium;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;

// ==============================================================================
// INET LEOSATELLITES IMPORTS
// ==============================================================================
import inet_leosatellites.networklayer.configurator.ipv4.SatelliteNetworkConfiguratorScs;

// ==============================================================================
// SIMU5G IMPORTS
// ==============================================================================
// import simu5g.nodes.Upf;
// import simu5g.nodes.NR.gNodeB;
import simu5g.nodes.eNodeB;
import simu5g.nodes.PgwStandard;
import simu5g.common.binder.Binder;
import simu5g.world.radio.LteChannelControl;
import simu5g.common.carrierAggregation.CarrierAggregation;

// ==============================================================================
// SCS UTILS IMPORTS
// ==============================================================================
import scs_utils.converter.PositionConverter;

// ==============================================================================
// SCS OPTIONAL IMPORTS (Veins Integration)
// ==============================================================================
import scs_optional.veins_inet_scs.VeinsInetManager;
import scs_optional.veins_scs.visualizer.RoadsINETCanvasVisualizer;

// ==============================================================================
// VEINS IMPORTS
// ==============================================================================
import org.car2x.veins.visualizer.roads.RoadsCanvasVisualizer;

// ==============================================================================
// HYBRID FUSION NETWORK
// Satellite-Vehicle 5G/LTE Hybrid Network with Dual Connectivity
// ==============================================================================
network HybridFusionNet
{
    parameters:
        int numOfSats = default(1); // Number of satellites
        int numOfMCCs = default(1); // Number of Mission Control Centers
        int numOfServiceLinkChannels = default(1); // Number of Service Link Channels (FDMA)
        // int numOfCars = default(0); // Number of GroundStations (if needed)
        // int numOfUECar = default(4);
        // int numUe = default(4);
        // int numUeD2DTx = default(0);
        // int numUeD2DRx = default(0);

        @display("bgi=veinsAndJapan1080,s;bgb=2160,1080");
        //! @display("bgi=veinsAndWorldMap,s;bgb=3660,1080");
   
    submodules:
        // ======================================================================
        // CORE NETWORK MODULES
        // ======================================================================
        channelControl: LteChannelControl { 
            @display("p=0,0;i=onedot"); 
        }

        routingRecorder: RoutingTableRecorder {
            @display("p=0,0;i=onedot"); 
        }

        configurator: SatelliteNetworkConfiguratorScs {
            parameters:
                @display("p=0,0;i=onedot");  
        }

        binder: Binder { 
            @display("p=0,0;i=onedot"); 
        }

        sbinder: SBinder { 
            @display("p=0,0;i=onedot"); 
        }

        carrierAggregation: CarrierAggregation { 
            @display("p=0,0;i=onedot"); 
        }

        visualizer: <default("IntegratedCanvasVisualizer")> like IIntegratedVisualizer { 
            @display("p=0,0;i=onedot"); 
        }
        
        // ======================================================================
        // VEINS INTEGRATION MODULES
        // ======================================================================
        manager: VeinsInetManager { 
            @display("p=0,0;i=onedot"); 
        }
        
        roadsCanvasVisualizer: RoadsINETCanvasVisualizer { 
            @display("p=0,0;i=onedot"); 
        }

        // ======================================================================
        // RADIO MEDIUM MODULES
        // ======================================================================
        radioMediumFL: <default("SatelliteScalarRadioMedium")> like IRadioMedium {
            parameters:
                @display("p=0,0;i=misc/sun");
        }

        radioMediumSV[numOfServiceLinkChannels]: <default("SatelliteScalarRadioMedium")> like IRadioMedium {
            parameters:
                @display("p=0,0;i=misc/sun");
        }

        // ======================================================================
        // COORDINATE CONVERTER
        // ======================================================================
        Pos: PositionConverter {
            parameters:
                @display("p=0,0;i=onedot");
        }

        // ======================================================================
        // MOBILE NODES (Hybrid Vehicles)
        // ======================================================================
        node[0]: HybridCarV2X;

        // ======================================================================
        // SATELLITE NODES
        // ======================================================================
        satellite[numOfSats]: LaserSatellite {  
            @display("p=797.72003,93.24;i=os3/satellit_blue"); 
        }

        // ======================================================================
        // GROUND STATIONS (Mission Control Centers)
        // ======================================================================
        MCC[numOfMCCs]: GroundStation { 
            @display("p=1284.64,88.06;r=10;i=device/receiverdish"); 
        }

        // ======================================================================
        // INTERNET BACKBONE
        // ======================================================================
        server: StandardHost {
            @display("p=100,1000;is=n;i=device/server");
        } 

        internetRouter: Router {
            @display("p=600,1000;i=device/smallrouter");
        } 

        // ======================================================================
        // CELLULAR INFRASTRUCTURE (4G LTE)
        // ======================================================================
        pgw: PgwStandard {
            @display("p=700,800;is=s");
        }

        eNB: eNodeB {
            @display("p=2000,1000;is=s");
        }

        // ======================================================================
        // CELLULAR INFRASTRUCTURE (5G NR) - Optional/Commented
        // ======================================================================
        // upf: Upf { // 5G User Plane Function
        //     @display("p=800,800");
        // }
        
        // gNB: gNodeB { // 5G Base Station
        //     @display("p=2000,1000;is=s");
        // }

        // ======================================================================
        // SATELLITE FEEDER LINK ROUTER
        // ======================================================================
        routerFL: Router {
            @display("p=1000,1000;i=device/smallrouter");
        }

    // ==========================================================================
    // NETWORK CONNECTIONS
    // ==========================================================================
    connections allowunconnected:  

        // Internet Backbone: Server <-> Internet Router
        server.pppg++ <--> Eth10G <--> internetRouter.pppg++;

        // Terrestrial Network (4G LTE): eNodeB <-> PGW <-> Internet Router
        internetRouter.pppg++ <--> Eth10G <--> pgw.filterGate;
        pgw.pppg++ <--> Eth10G <--> eNB.ppp;

        // Terrestrial Network (5G NR) - Optional/Commented
        // internetRouter.pppg++ <--> Eth10G <--> upf.filterGate;
        // upf.pppg++ <--> Eth10G <--> gNB.ppp; 

        // Satellite Network (NTN): MCC <-> Feeder Link Router <-> Internet Router
        internetRouter.pppg++ <--> Eth10G <--> routerFL.pppg++;
        routerFL.pppg++ <--> Eth10G <--> MCC[0].pppg++; 
}
