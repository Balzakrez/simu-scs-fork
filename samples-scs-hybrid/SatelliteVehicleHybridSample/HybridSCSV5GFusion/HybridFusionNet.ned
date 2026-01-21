//
// File: SV5GFusion4.ned - Network definition for Hybrid Satellite-Vehicle 5G Fusion Sample
// 

package SatelliteVehicleHybridSample.HybridSCSV5GFusion;

//custom imports
import SatelliteVehicleHybridSample.HybridSCSV5GFusion.HybridCarV2X;

// scs
import scs.base.GroundStation;
import scs.base.LaserSatellite;
import scs.common.binder.SBinder;

// inet
import inet.node.inet.Router;
import inet.node.ethernet.Eth10G;
import inet.node.inet.StandardHost;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.visualizer.contract.IIntegratedVisualizer;
import inet.physicallayer.wireless.apsk.packetlevel.ApskScalarRadioMedium;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;

// inet_leosatellites
import inet_leosatellites.networklayer.configurator.ipv4.SatelliteNetworkConfiguratorScs;

// simu5G
// import simu5g.nodes.Ue;
// import simu5g.nodes.Upf;
// import simu5g.nodes.NR.gNodeB;
import simu5g.nodes.eNodeB;
import simu5g.nodes.PgwStandard;
import simu5g.common.binder.Binder;
import simu5g.world.radio.LteChannelControl;
import simu5g.common.carrierAggregation.CarrierAggregation;

//scs_utils
import scs_utils.converter.PositionConverter;

//veins + scs_optional
// import scs_optional.node.UECar;
// import scs_optional.veins_inet_scs.InetCar;
import scs_optional.veins_inet_scs.VeinsInetManager;
import org.car2x.veins.visualizer.roads.RoadsCanvasVisualizer;
import scs_optional.veins_scs.visualizer.RoadsINETCanvasVisualizer;

// SV5GFusion4 Network 
network HybridFusionNet
{
    parameters:
        int numOfSats = default(1); // Number of satellites
        int numOfMCCs = default(1); // Number of Mission Control Centers
        int numOfServiceLinkChannels = default(1); // Number of Service Link Channels (Satellite -> )
        // int numOfCars = default(0); // Number of GroundStations (necessary)
    
        // int numOfUECar = default(4);
        // int numUe = default(4);
        // int numUeD2DTx = default(0);
        // int numUeD2DRx = default(0);

        @display("bgi=veinsAndJapan1080,s;bgb=2160,1080");
   
    submodules:
        // ===========================================================================

        channelControl: LteChannelControl { 
            @display("p=0,0;i=onedot"); 
        }

        routingRecorder: RoutingTableRecorder {
            @display("p=0,0;i=onedot"); 
        }

        configurator: SatelliteNetworkConfiguratorScs {
            parameters:
                @display("p=0,0;i=onedot");  
                // Todo: Specify parameters for satellite-ground link (es. max distance, etc)
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

        manager: VeinsInetManager { 
            @display("p=0,0;i=onedot"); 
        }
        
        roadsCanvasVisualizer: RoadsINETCanvasVisualizer { 
            @display("p=0,0;i=onedot"); 
        }
        
        radioMediumFL: <default("SatelliteScalarRadioMedium")> like IRadioMedium {
            parameters:
                //numChannels = 2;
                @display("p=0,0;i=misc/sun");
        }

        radioMediumSV[numOfServiceLinkChannels]: <default("SatelliteScalarRadioMedium")> like IRadioMedium {
            parameters:
                //numChannels = 2;
                @display("p=0,0;i=misc/sun");
        }

        Pos: PositionConverter {
              parameters:
                //numChannels = 2;
                @display("p=40,0;i=onedot");
        }
        // ===========================================================================

        node[0]: HybridCarV2X; // or use UECar;

        // Use "LaserSatellite" for using NoradA 
        satellite[numOfSats]: LaserSatellite {  
            @display("p=797.72003,93.24;i=os3/satellit_blue"); 
        }

        MCC[numOfMCCs]: GroundStation { 
            @display("p=1284.64,88.06;r=10;i=device/receiverdish"); 
        }

        // ===========================================================================

        server: StandardHost {
            @display("p=100,1100;is=n;i=device/server");
        } 

        internetRouter: Router {
            @display("p=400,1100;i=device/smallrouter");
        } 

        // 4G LTE Architecture
        pgw: PgwStandard {
            @display("p=700,800;is=s");
        }

        eNB: eNodeB {
                @display("p=2000,1000;is=s");
        }

        // 5G Architecture
        // upf: Upf { // 5G User Plane Function
        //     @display("p=800,800");
        // }
        
        // gNB: gNodeB { // 5G Base Station
        //         @display("p=2000,1000;is=s");
        // }

        routerFL: Router {
            @display("p=1100,1100;i=device/smallrouter");
        }

   connections allowunconnected:  

        // Router -> Server
        server.pppg++ <--> Eth10G <--> internetRouter.pppg++;

        // Terrestrial (5G/TN): gNodeB -> upf -> Internet
        // 5G Core
        // internetRouter.pppg++ <--> Eth10G <--> upf.filterGate;
        // upf.pppg++ <--> Eth10G <--> gNB.ppp; 

        // Terrestrial (4G LTE/TN): gNodeB -> pgw -> Internet
        // 4G LTE Core
        internetRouter.pppg++ <--> Eth10G <--> pgw.filterGate;
        pgw.pppg++ <--> Eth10G <--> eNB.ppp;

        // Satellite (NTN): MCC (Gateway) -> Router Feeder Link -> Internet
        internetRouter.pppg++ <--> Eth10G <--> routerFL.pppg++;
        routerFL.pppg++ <--> Eth10G <--> MCC[0].pppg++; 
}