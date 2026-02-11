#!/bin/bash

set -euo pipefail

# ==============================================================================
# CONFIGURATION
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUN_SCRIPT="${SCRIPT_DIR}/run.sh"
LOG_DIR="${SCRIPT_DIR}/logs"

SIM_TIME=""
NO_LOGS="false"
MAX_PARALLEL=4
SELECTED_SET="all"
SUMO_BASE_PORT=9999

# ==============================================================================
# SIMULATION SETS
# ==============================================================================

declare -a SINGLE_ALL=(
    "CellularOnlyBaseline"
    "CellularOnlyWithTraffic"
    "SatelliteOnlyBaseline"
    "SatelliteOnlyWithTraffic"
)


#"QoSBasedBaseline"
declare -a QOS_ALL=(
    "QoSBasedBaseline"
    # "QoSBasedLatencySensitive"
    # "QoSBasedReliabilityFocused"
    # "QoSBasedThroughputOptimized"
    # "QoSBasedAggressive"
)

#"EnergyAwareBaseline"
declare -a ENERGY_ALL=(
    # "EnergyAwareMinimizeEnergy"
    # "EnergyAwareMaximizeQoS"
    # "EnergyAwareBalanced"
    # "EnergyAwareLowBattery"
    # "EnergyAwareSwitchingWithTraffic"
)

# --- All 17 configurations ---
declare -a SET_ALL=(
    # "CellularOnlyBaseline"
    # "CellularOnlyWithTraffic"
    # "SatelliteOnlyBaseline"
    # "SatelliteOnlyWithTraffic"

    # "TimeBasedBaseline"
    # "TimeBasedSwitchingPing"
    # "TimeBasedSwitchingFast"
    # "TimeBasedSwitchingStartSat"

    # "CoverageBasedBaseline"
    # "CoverageBasedWithTraffic"
    # "CoverageBasedWithTraffic200"

    "QoSBasedBaseline"
    # "QoSBasedLatencySensitive"
    # "QoSBasedReliabilityFocused"
    # "QoSBasedThroughputOptimized"
    # "QoSBasedAggressive"
    
    # "EnergyAware_100E_00Q"
    # "EnergyAware_90E_10Q"
    # "EnergyAware_80E_20Q"
    # "EnergyAware_70E_30Q"
    # "EnergyAware_60E_40Q"
    # "EnergyAware_50E_50Q"
    # "EnergyAware_40E_60Q"
    # "EnergyAware_30E_70Q"
    # "EnergyAware_20E_80Q"
    # "EnergyAware_10E_90Q"
    # "EnergyAware_00E_100Q"
)

# ==============================================================================
# FUNCTIONS
# ==============================================================================

usage() {
    echo "============================================================"
    echo " batch_run.sh - Hybrid TN-NTN Batch Simulation Runner"
    echo "============================================================"
    echo ""
    echo "Usage:"
    echo "  ./runBatch.sh [options]"
    echo ""
    echo "Options:"
    echo "  --set <name>       Simulation set to run:"
    echo "                     single      - Cellular + Satellite"
    echo "                     qos         - QoS-based configurations"
    echo "                     energy      - Energy-aware configurations"
    echo "                     all         - All configurations (default)"
    echo ""
    echo "  --parallel <N>     Max concurrent simulations (default: 4)"
    echo "  -t <time>          Simulation time limit (e.g., 500s, 10min)"
    echo "  --no-logs          Disable logging (default: false)"
    echo "  --help             Show this help"
    echo ""
    echo "Examples:"
    echo "  ./runBatch.sh --set single --parallel 4 -t 500s"
    echo "  ./runBatch.sh --set qos --parallel 4 -t 500s"
    echo "  ./runBatch.sh --set energy --parallel 4 -t 500s"
    echo "  ./runBatch.sh --set all --parallel 4 -t 5000s"
    echo "============================================================"
}

# Select configuration set based on --set argument
get_set() {
    local set_name="$1"
    case "$set_name" in
        single)     echo "${SINGLE_ALL[@]}" ;;
        qos)        echo "${QOS_ALL[@]}" ;;
        energy)     echo "${ENERGY_ALL[@]}" ;;
        all)        echo "${SET_ALL[@]}" ;;
        *)
            echo "ERROR: Unknown set '$set_name'" >&2
            echo "Available: single, qos, energy, all" >&2
            exit 1
            ;;
    esac
}

# Run a single simulation configuration
run_simulation() {
    local config="$1"
    local index="$2"         
    local sumo_port=$((SUMO_BASE_PORT + index))
    local log_file="${LOG_DIR}/${config}.log"
    local start_time
    local end_time
    local elapsed

    # Check if logs are disabled
    local output_dest
    if [[ "$NO_LOGS" == "true" ]]; then
        output_dest="/dev/null"
    else
        output_dest="$log_file"
    fi

    start_time=$(date +%s)
    echo "[$(date '+%H:%M:%S')] [START]  $config (SUMO port: $sumo_port)"

    # Make command arguments array
    local cmd_args=("-c" "$config" "--sumo-port" "$sumo_port")
    
    # Check if SIM_TIME is set
    if [[ -n "$SIM_TIME" ]]; then
        cmd_args+=("-t" "$SIM_TIME")
    fi

    # Run the simulation with appropriate logging
    # Stdout and stderr redirection to log file or /dev/null
    if "$RUN_SCRIPT" "${cmd_args[@]}" > "$output_dest" 2>&1; then
        end_time=$(date +%s)
        elapsed=$((end_time - start_time))
        echo "[$(date '+%H:%M:%S')] [OK]     $config (${elapsed}s)"
    else
        end_time=$(date +%s)
        elapsed=$((end_time - start_time))
        if [[ "$NO_LOGS" == "true" ]]; then
            echo "[$(date '+%H:%M:%S')] [FAILED] $config (${elapsed}s) - (Logs disabled)"
        else
            echo "[$(date '+%H:%M:%S')] [FAILED] $config (${elapsed}s) - check ${log_file}"
        fi
        return 1
    fi
}


# ==============================================================================
# ARGUMENT PARSING
# ==============================================================================

while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --set)
            SELECTED_SET="$2"
            shift 2
            ;;
        --parallel)
            MAX_PARALLEL="$2"
            shift 2
            ;;
        -t)
            SIM_TIME="$2"
            shift 2
            ;;
        --no-logs)       
            NO_LOGS="true"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done


# ==============================================================================
# VALIDATION
# ==============================================================================

if [[ ! -f "$RUN_SCRIPT" ]]; then
    echo "ERROR: run.sh not found at $SCRIPT_DIR"
    exit 1
fi

# Get selected configurations
CONFIGS=($(get_set "$SELECTED_SET"))
TOTAL=${#CONFIGS[@]}

if [[ $TOTAL -eq 0 ]]; then
    echo "ERROR: No configurations found for set '$SELECTED_SET'"
    exit 1
fi

# Create log directory
mkdir -p "$LOG_DIR"


# ==============================================================================
# SUMMARY
# ==============================================================================

echo "============================================================"
echo " Batch Simulation Runner"
echo "============================================================"
echo " Set:        $SELECTED_SET"
echo " Configs:    $TOTAL"
echo " Parallel:   $MAX_PARALLEL"
echo " Sim Time:   $SIM_TIME"
echo " No Logs:    $NO_LOGS"
echo " Log Dir:    $LOG_DIR"
echo "============================================================"
echo " Configurations:"
for i in "${!CONFIGS[@]}"; do
    printf "   %2d. %s\n" $((i+1)) "${CONFIGS[$i]}"
done
echo "============================================================"
echo ""

# ==============================================================================
# EXECUTION (parallel with job limit)
# ==============================================================================

FAILED=0
SUCCEEDED=0
job_count=0
index=0
for config in "${CONFIGS[@]}"; do
    run_simulation "$config" "$index" & job_count=$((job_count + 1))
    
    index=$((index + 1))

    if [[ $job_count -ge $MAX_PARALLEL ]]; then
        wait -n 2>/dev/null || true
        job_count=$((job_count - 1))
    fi
done

wait # Wait for all remaining jobs

# ==============================================================================
# SUMMARY
# ==============================================================================

echo ""
echo "============================================================"
echo " Batch Complete"
echo " Total: $TOTAL | Logs: $LOG_DIR"
echo "============================================================"