//
// File: SV5GFusion4.ned - Network definition for Hybrid Satellite-Vehicle 5G Fusion Sample
// 

package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

// ==============================================================================
// CUSTOM IMPORTS
// ==============================================================================
// import SatelliteVehicleHybridSample.HybridSCSV5GFusion.HybridCarV2X4G;
import SatelliteVehicleHybridSample.HybridSCSV5GFusion.HybridCarV2X5G;

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
import simu5g.nodes.Upf;
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
network HybridFusionNet4G
{
    parameters:
        int numOfSats = default(1); // Number of satellites
        int numOfMCCs = default(1); // Number of Mission Control Centers
        int numOfServiceLinkChannels = default(1); // Number of Service Link Channels

        @display("bgi=veinsAndJapan1080,s;bgb=2160,1080");
        // @display("bgi=veinsAndWorldMap,s;bgb=3660,1080");
   
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
			moduleType = "SatelliteVehicleHybridSample.HybridSCSV5GFusion.HybridCarV2X4G";
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
        node[0]: HybridCarV2X4G;

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
        // CELLULAR INFRASTRUCTURE (5G NR) 
        // ======================================================================
        upf: Upf { // 5G User Plane Function
            @display("p=800,1000");
        }
        
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

        // 1. INTERNET BACKBONE
        // Flow: Server -> Internet -> InternetRouter
        server.pppg++ <--> Eth10G <--> internetRouter.pppg++;

        // ----------------------------------------------------------------------
        // 2. Terrestrial Network (TN) - 4G LTE
        // Flow: InternetRouter -> PGW -> eNodeB -> HybridCarV2X
        // ----------------------------------------------------------------------
        // The PGW connects the Internet to the LTE network
        internetRouter.pppg++ <--> Eth10G <--> pgw.filterGate;
        // The eNodeB connects to the PGW
        pgw.pppg++ <--> Eth10G <--> eNB.ppp;
        // ----------------------------------------------------------------------
        // 3. Satellite Network (NTN) - 5G Core
        // Flow: InternetRouter -> UPF -> RouterFL -> MCC -> Satellite -> HybridCarV2X
        // ----------------------------------------------------------------------
        // The UPF acts as a gateway between the Internet and the satellite network
        internetRouter.pppg++ <--> Eth10G <--> upf.pppg++;
        // The output of the UPF goes to the router that manages the satellite links
        upf.pppg++ <--> Eth10G <--> routerFL.pppg++;
        // The satellite router connects to the Ground Station (MCC)
        routerFL.pppg++ <--> Eth10G <--> MCC[0].pppg++; 
}
