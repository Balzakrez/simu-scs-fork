//
// File: HybridFusionNet5G.ned - Network definition for Hybrid Satellite-Vehicle 5G NR Fusion Sample
// 

package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

// ==============================================================================
// CUSTOM IMPORTS
// ==============================================================================
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
import simu5g.nodes.NR.gNodeB;
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
// HYBRID FUSION NETWORK 5G
// Satellite-Vehicle 5G NR Hybrid Network with Dual Connectivity
// ==============================================================================
network HybridFusionNet5G
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
            parameters:
            	@display("p=0,0;i=onedot"); 
        }

        routingRecorder: RoutingTableRecorder {
            parameters:
            	@display("p=0,0;i=onedot"); 
        }

        configurator: SatelliteNetworkConfiguratorScs {
            parameters:
                @display("p=0,0;i=onedot");  
        }

        binder: Binder { 
            parameters:
            	@display("p=0,0;i=onedot"); 
        }

        sbinder: SBinder { 
            parameters:
            	@display("p=0,0;i=onedot"); 
        }

        carrierAggregation: CarrierAggregation { 
            parameters:
            	@display("p=0,0;i=onedot"); 
        }

        visualizer: <default("IntegratedCanvasVisualizer")> like IIntegratedVisualizer { 
            parameters:
            	@display("p=0,0;i=onedot"); 
        }
        
        // ======================================================================
        // VEINS INTEGRATION MODULES
        // ======================================================================
        manager: VeinsInetManager { 
            parameters:
            @display("p=0,0;i=onedot"); 
                moduleType = "SatelliteVehicleHybridSample.HybridSCSV5GFusion.HybridCarV2X5G";
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
        // MOBILE NODES (Hybrid Vehicles with 5G NR)
        // ======================================================================
        node[0]: HybridCarV2X5G;

        // ======================================================================
        // SATELLITE NODES
        // ======================================================================
        satellite[numOfSats]: LaserSatellite { 
            parameters: 
            	@display("p=797.72003,93.24;i=os3/satellit_blue"); 
        }

        // ======================================================================
        // GROUND STATIONS (Mission Control Centers)
        // ======================================================================
        MCC[numOfMCCs]: GroundStation { 
            parameters:
            	@display("p=1284.64,88.06;r=10;i=device/receiverdish"); 
        }

        // ======================================================================
        // INTERNET BACKBONE
        // ======================================================================
        server: StandardHost {
            parameters:
            	@display("p=100,1000;is=n;i=device/server");
        } 

        internetRouter: Router {
            parameters:
            	@display("p=300,1000;i=device/smallrouter");
        } 

        // ======================================================================
        // CELLULAR INFRASTRUCTURE (5G NR)
        // ======================================================================
        upf: Upf {
            parameters:
            	@display("p=500,700;is=s");
        }

        upfNTN: Upf {
            parameters:
            	@display("p=700,1000;is=s");
        }

        gNB: gNodeB {
            parameters:
            	@display("p=2000,1000;is=s");
        }

        // ======================================================================
        // SATELLITE FEEDER LINK ROUTER
        // ======================================================================
        routerFL: Router {
            parameters:
            	@display("p=1000,1000;i=device/smallrouter");
        }

    // ==========================================================================
    // NETWORK CONNECTIONS
    // ==========================================================================
    connections allowunconnected:  

        // Internet Backbone: Server <-> Internet Router
        server.pppg++ <--> Eth10G <--> internetRouter.pppg++;

        // Terrestrial Network (5G NR): gNodeB <-> UPF <-> Internet Router
        internetRouter.pppg++ <--> Eth10G <--> upf.filterGate;
        upf.pppg++ <--> Eth10G <--> gNB.ppp;

        // Satellite Network (NTN): MCC <-> Feeder Link Router <-> UPF <-> Internet Router
        internetRouter.pppg++ <--> Eth10G <--> upfNTN.pppg++;
        upfNTN.pppg++ <--> Eth10G <--> routerFL.pppg++;
        routerFL.pppg++ <--> Eth10G <--> MCC[0].pppg++; 
}