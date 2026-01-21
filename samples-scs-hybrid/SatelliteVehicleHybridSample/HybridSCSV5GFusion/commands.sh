# Run this script from the HybridSCSV5GFusion folder
#   cd simu-scs-fork/samples-scs-hybrid/SatelliteVehicleHybridSample/HybridSCSV5GFusion
#
# Use CMDENV for faster execution without GUI (recommended for large-scale simulations)
#   because GUI visualization slows down the simulation significantly and crashes with many nodes (usually).
#   Note: Using WSL is recommended CMDENV for faster execution without GUI and avoid potential GUI issues.
#   
# Use QTENV for GUI visualization and debugging (slower execution).


# STEP 1: Start SUMO traffic simulator 
# Use sumo if you want to run without GUI (recommended)
sumo --remote-port 9999 --num-clients 1 -c config.sumocfg
# or use if you want the GUI visualization 
sumo-gui --remote-port 9999 --num-clients 1 -c config.sumocfg

# STEP 2: Start OMNeT++ simulation with SUMO connection
#   - QTENV is used to run with GUI for visualization and debugging
#   - CMDENV is used to run without GUI for faster execution and batch runs (recommended)

# QTENV with configuration selection
../../samples-scs-hybrid_dbg -m -u Qtenv \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 \
    -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins \
    -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini
    
# CMDENV CellularOnly (NO GUI)
../../samples-scs-hybrid_dbg -u Cmdenv -m \
    -c CellularOnly \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini

# CMDENV SatelliteOnlyBurstTraffic (NO GUI)
../../samples-scs-hybrid_dbg -u Cmdenv -m -c SatelliteOnlyBurstTraffic \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x "inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars" \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 \
    -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins \
    -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini

# CMDENV SatelliteOnlyVehicleTelemetryVariable (NO GUI)
../../samples-scs-hybrid_dbg -u Cmdenv -m -c SatelliteOnlyVehicleTelemetryVariable  \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x "inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars" \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 \
    -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins \
    -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini

# CMDENV TimeBasedSwitching (NO GUI)
../../samples-scs-hybrid_dbg -u Cmdenv -m -c TimeBasedSwitching \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x "inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars" \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 \
    -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins \
    -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini


# CMDENV TimeBasedSwitchingFast (NO GUI)
../../samples-scs-hybrid_dbg -u Cmdenv -m -c TimeBasedSwitchingFast \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x "inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars" \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 \
    -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins \
    -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini


# CMDENV CoverageBasedSwitching (NO GUI)
../../samples-scs-hybrid_dbg -u Cmdenv -m -c CoverageBasedSwitching \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x "inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars" \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 \
    -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins \
    -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini

# CMDENV EnergyBasedAggressiveDrainage (NO GUI)
../../samples-scs-hybrid_dbg -u Cmdenv -m -c EnergyBasedAggressiveDrainage \
    -n ../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src \
    -x "inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars" \
    --image-path=../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images \
    -l ../../../external/inet/src/INET \
    -l ../../../modules/scs/src/scs \
    -l ../../../modules/inet_leosatellites/src/inet_leosatellites \
    -l ../../../external/leosatellites/src/leosatellites \
    -l ../../../modules/os3/src/os3 \
    -l ../../../modules/scs_utils/src/scs_utils \
    -l ../../../external/veins/src/veins \
    -l ../../../scs_optional/src/scs_optional \
    -l ../../../external/simu5G/src/simu5g \
    omnetpp.ini