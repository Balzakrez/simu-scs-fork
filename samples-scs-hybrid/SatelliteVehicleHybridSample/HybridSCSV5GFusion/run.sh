#!/bin/bash
# run.sh - Script to run SUMO + OMNeT++ simulations with simplified commands

# Common paths
EXEC="../../samples-scs-hybrid_dbg"
NED_PATH="../..:../../../external/inet/examples:../../../external/inet/showcases:../../../external/inet/src:../../../external/inet/tests/validation:../../../external/inet/tests/networks:../../../external/inet/tutorials:../../../modules/scs/src/scs:../../../modules/scs/src/inet:../../../modules/inet_leosatellites/src/inet:../../../external/leosatellites/src/leosatellites:../../../modules/os3/src/os3:../../../modules/scs_utils/src/scs_utils:../../../external/veins/examples/veins:../../../external/veins/src/veins:../../../modules/os3_leosatellites/src/os3:../../../scs_optional/src/veins_scs:../../../scs_optional/src/veins_inet_scs:../../../scs_optional/src/node:../../../external/simu5G/emulation:../../../external/simu5G/simulations:../../../external/simu5G/src"

EXCLUDE="inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream;simu5g.simulations.LTE.cars;simu5g.simulations.NR.cars;simu5g.nodes.cars"

IMAGE_PATH="../../images:../../../external/inet/images:../../../external/veins/images:../../../external/simu5G/images"

LIBS="-l ../../../external/inet/src/INET \
-l ../../../modules/scs/src/scs \
-l ../../../modules/inet_leosatellites/src/inet_leosatellites \
-l ../../../external/leosatellites/src/leosatellites \
-l ../../../modules/os3/src/os3 \
-l ../../../modules/scs_utils/src/scs_utils \
-l ../../../external/veins/src/veins \
-l ../../../scs_optional/src/scs_optional \
-l ../../../external/simu5G/src/simu5g"

# SUMO configuration
SUMO_CONFIG="../sumodir/config.sumocfg"
SUMO_PORT=9999

# Function to display help
show_help() {
    echo "@This script run SUMO on background and start OMNeT++ simulation with specified configuration."
    echo "@See commands.sh file for more details and additional configuration options."
    echo ""
    echo "- Usage:"
    echo "      ./run.sh -c <config> [options]"
    echo ""
    echo "- Required:"
    echo "      -c <config>    Simulation configuration name"
    echo ""
    echo "- Optional parameters:"
    echo "      --sumo-gui     Use SUMO with GUI (default: no GUI)"
    echo "      --debug        Enable debug mode on errors"
    echo "      -t <time>      Set simulation time limit (e.g., 1000s, 10min)"
    echo "      -h, --help     Show this help message"
    echo ""
    echo "Examples usage:"
    echo "  ./run.sh -c TimeBasedSwitchingFast"
    echo "  ./run.sh -c CoverageBasedSwitching --sumo-gui"
    echo "  ./run.sh -c EnergyBasedAggressiveDrainage --debug -t 1000s"
    echo "  ./run.sh -c CoverageBasedSwitching --sumo-gui --debug -t 1000s"
    echo "  ./run.sh -h"
}

# Cleanup function to kill SUMO on exit
cleanup() {
    echo ""
    echo "Cleaning up..."
    if [ ! -z "$SUMO_PID" ]; then
        echo "Stopping SUMO (PID: $SUMO_PID)..."
        kill $SUMO_PID 2>/dev/null
    fi
}

# Register cleanup function
trap cleanup EXIT INT TERM

# Check for help flag first
if [ "$1" == "-h" ] || [ "$1" == "--help" ] || [ $# -eq 0 ]; then
    show_help
    exit 0
fi

# Initialize variables
CONFIG=""
EXTRA_OPTS="-m"
SUMO_CMD="sumo"

# Parse arguments
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c)
            if [ -z "$2" ]; then
                echo "ERROR: -c requires a configuration name"
                show_help
                exit 1
            fi
            CONFIG=$2
            shift 2
            ;;
        --sumo-gui)
            SUMO_CMD="sumo-gui"
            shift
            ;;
        --debug)
            EXTRA_OPTS="$EXTRA_OPTS --debug-on-errors=true"
            echo "Debug mode: enabled"
            shift
            ;;
        -t)
            if [ -z "$2" ]; then
                echo "ERROR: -t requires a time limit value"
                show_help
                exit 1
            fi
            EXTRA_OPTS="$EXTRA_OPTS --sim-time-limit=$2"
            echo "Simulation time limit: $2"
            shift 2
            ;;
        *)
            echo "ERROR: Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Check if config was specified
if [ -z "$CONFIG" ]; then
    echo "ERROR: Configuration name is required (use -c <config>)"
    show_help
    exit 1
fi

# Check if SUMO config exists
if [ ! -f "$SUMO_CONFIG" ]; then
    echo "ERROR: SUMO config file '$SUMO_CONFIG' not found!"
    exit 1
fi

# Check if port is already in use
if lsof -Pi :$SUMO_PORT -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo "WARNING: Port $SUMO_PORT is already in use!"
    read -p "Kill existing process and continue? (y/n): " choice
    if [ "$choice" == "y" ]; then
        PID=$(lsof -t -i:$SUMO_PORT)
        kill $PID 2>/dev/null
        sleep 1
    else
        echo "Aborted."
        exit 1
    fi
fi

echo "========================================="
echo "STEP 1: Starting SUMO traffic simulator"
echo "========================================="
echo "SUMO executable: $SUMO_CMD"
echo "SUMO config: $SUMO_CONFIG"
echo "Remote port: $SUMO_PORT"
echo ""

# Start SUMO in background
$SUMO_CMD --remote-port $SUMO_PORT --num-clients 1 -c $SUMO_CONFIG &
SUMO_PID=$!

echo "SUMO started (PID: $SUMO_PID)"
echo "Waiting for SUMO to initialize..."
sleep 2

# Check if SUMO is still running
if ! ps -p $SUMO_PID > /dev/null; then
    echo "ERROR: SUMO failed to start!"
    exit 1
fi

echo ""
echo "========================================="
echo "STEP 2: Starting OMNeT++ simulation"
echo "========================================="
echo "Configuration: $CONFIG"
echo "User interface: Cmdenv (no GUI)"
echo ""

# Run OMNeT++ simulation
$EXEC -u Cmdenv $EXTRA_OPTS -c $CONFIG \
    -n "$NED_PATH" \
    -x "$EXCLUDE" \
    --image-path="$IMAGE_PATH" \
    $LIBS \
    omnetpp.ini

echo ""
echo "Simulation completed!"