# simu-scs-fork

Fork of the [simu-scs](https://github.com/ToyotaInfoTech/simu-scs) LEO Satellite Communication Simulation Framework with patches and improvements for hybrid satellite-cellular vehicular networks.

## Overview

This fork extends the original simu-scs framework with the following key features:

- **HybridCarV2X module** - Vehicles can communicate via both satellite and cellular interfaces
- **Direct V2Sat communication** - Vehicles equipped with satellite antennas can transmit directly to LEO satellites
- **Interface switching strategies** - Dynamic selection between terrestrial and non-terrestrial networks based on configurable criteria

Based on the research framework described in:
> Jing Ma, Lei Zhong, and Ryokichi Onishi, "LEO Satellite Communication Simulation Framework for Connected Vehicles," IEEE GLOBECOM 2023.

---

## Prerequisites

### System Requirements

- **Operating System**: Linux (Ubuntu 20.04+ recommended) or WSL2
- **OMNeT++ 6.0.3**
- **SUMO 1.18**
- **Git** with submodule support
- **Build tools**: See detailed dependencies below

**Framework Dependencies** (auto-installed by `prepare_dependencies.sh`):
- INET Framework 4.4.1
- Veins 5.2
- Simu5G 1.2.1
- leosatellites (master + v2.0.0 physicallayer)
- os3 (master)

### OMNeT++ 6.0.3 Installation

#### 1. Install System Dependencies

```bash
# Core build tools and libraries
sudo apt install -y make diffutils pkg-config ccache clang lld gdb lldb \
    bison flex perl sed gawk python3 python3-pip python3-venv python3-dev \
    libxml2-dev zlib1g-dev doxygen graphviz xdg-utils libdw-dev \
    cmake wget

# Java (required for OMNeT++ IDE)
sudo apt install -y openjdk-17-jre openjdk-17-jdk

# Qt5 libraries (for GUI)
sudo apt install -y qtbase5-dev qtbase5-dev-tools libqt5svg5 qtwayland5 \
    libwebkit2gtk-4.1-0 libqt5opengl5-dev

# OpenSceneGraph (for 3D visualization)
sudo apt install -y libopenscenegraph-dev

# Clean up
sudo apt clean
```

**Verify Java installation:**
```bash
java -version
# Should output: openjdk version "17.x.x" or similar
```

#### 2. Enable ptrace for Debugging (Optional but Recommended)

```bash
sudo nano /etc/sysctl.d/10-ptrace.conf
# Change the line to: kernel.yama.ptrace_scope = 0
# Save and exit, then reload:
sudo sysctl --system
```

#### 3. Install Python Dependencies

```bash
python3 -m pip install numpy scipy pandas matplotlib posix_ipc --break-system-packages
```

#### 4. Download and Install OMNeT++ 6.0.3

```bash
# Download OMNeT++ 6.0.3 from https://github.com/omnetpp/omnetpp/releases/tag/omnetpp-6.0.3
# Extract the archive
tar xvfz omnetpp-6.0.3-linux-x86_64.tgz
cd omnetpp-6.0.3

# Set environment variables
source setenv

# Configure and build
./configure
make -j$(nproc)

# Test installation
cd samples/aloha
./aloha
```

**Important**: Always run `source setenv` in each new terminal before working with OMNeT++, or add it to your `.bashrc`:

```bash
echo "source ~/omnetpp-6.0.3/setenv" >> ~/.bashrc
```

---

## Installation

### 1. Clone the Repository

```bash
git clone --recursive https://github.com/Balzakrez/simu-scs-fork.git
cd simu-scs-fork
```

### 2. Install Dependencies

Run the `prepare_dependencies.sh` script to download and patch all external modules:

```bash
./prepare_dependencies.sh
```

This script will:
- Download **INET 4.4.1**, **Veins 5.2**, **Simu5G 1.2.1**
- Download **leosatellites** (master + v2.0.0 physicallayer) and **os3** (master)
- Apply compatibility patches for **leosatellites**, **INET**, **os3**, and **Simu5G**

### 3. Import into OMNeT++ IDE

1. Launch **OMNeT++ IDE** and create a new workspace

2. When OMNeT++ prompts to install INET Framework and examples:
   - ⚠️ **DO NOT INSTALL** - Uncheck both options and click OK
   - Reason: This project uses a specific INET 4.4.1 version (auto-installed by `prepare_dependencies.sh`)
   - Installing the default INET will break the build

3. Go to **File → Import → Existing Projects into Workspace**
4. Select the `simu-scs-fork/` directory as root directory
5. **Check all projects** in the list and click **Finish**
6. Wait for the IDE to complete indexing before building

### 4. Build the Project

In OMNeT++ IDE:
```
Project → Build All
```

---

## Project Structure

```
simu-scs-fork/
├── external/                 # External dependencies (INET, Veins, Simu5G, leosatellites, os3)
├── modules/                  # Custom modules (hybrid V2X, os3 integration)
├── patches/                  # Compatibility patches
├── samples-scs/              # Original simu-scs examples
├── samples-scs-hybrid/       # Hybrid satellite-cellular scenarios
├── README.md
└── prepare_dependencies.sh   # Dependency setup script
```

---

## Running Simulations

### Simulation Modes

**QTENV** - GUI visualization (slower, good for debugging):
- Shows vehicles, satellites, and network links visually
- Interactive debugging capabilities
- May crash with many nodes or on WSL without proper X11 setup

**CMDENV** - Command-line interface (faster, recommended for production):
- No GUI overhead - significantly faster execution
- Better for batch runs and parameter sweeps
- Recommended for WSL environments
- Outputs results to console and result files

### Hybrid Satellite-Cellular Scenarios

These examples demonstrate the hybrid V2X communication system with interface switching.

#### Setup

All commands should be run from the hybrid sample directory:
```bash
cd samples-scs-hybrid/SatelliteVehicleHybridSample/HybridSCSV5GFusion
```

#### Available Configurations

1. **CellularOnly** - Vehicles communicate only via 5G cellular network
2. **SatelliteOnly** - Vehicles communicate only via satellite links
3. **SatelliteOnlyBurstTraffic** - Satellite with burst traffic pattern
4. **SatelliteOnlyVehicleTelemetryVariable** - Variable telemetry data rates
5. **HybridSwitching** - Dynamic switching between cellular and satellite interfaces

#### Running Hybrid Scenarios

**Step 1**: Start SUMO
```bash
# Without GUI (recommended)
sumo --remote-port 9999 --num-clients 1 -c config.sumocfg

# Or with GUI
sumo-gui --remote-port 9999 --num-clients 1 -c config.sumocfg
```

**Step 2**: Run simulation

**With GUI visualization (QTENV):**
```bash
../../samples-scs-hybrid_dbg -u Qtenv -m omnetpp.ini
```

**Without GUI - HybridSwitching (CMDENV):**
```bash
../../samples-scs-hybrid_dbg -u Cmdenv -m -c HybridSwitching omnetpp.ini
```

> **Note**: For complete commands with all library paths and other configurations (CellularOnly, SatelliteOnly, SatelliteOnlyBurstTraffic, SatelliteOnlyVehicleTelemetryVariable), see `samples-scs-hybrid/SatelliteVehicleHybridSample/HybridSCSV5GFusion/commands.sh`

### Simulation Output

Results are saved in the `results/` directory:
- `.sca` files - Scalar results (statistics)
- `.vec` files - Vector results (time series data)
- `.vci` files - Vector index
- `.elog` files - Event log (if enabled)

Analyze results with OMNeT++ IDE Analysis Tool or export to CSV/MATLAB for custom analysis.

---

## Documentation

For complete architecture details, RF link models, and sample simulations, refer to the [Original simu-scs Documentation](simu-scs-readme.md).

---

## Contributing

This fork is part of ongoing research. Contributions and issue reports are welcome!

## License

This project inherits the LGPL-3.0 license from the original simu-scs framework.

```
Copyright (C) 2023 TOYOTA MOTOR CORPORATION
SPDX-License-Identifier: LGPL-3.0-or-later
```

---

## Credits

- **Original Framework**: [simu-scs](https://github.com/ToyotaInfoTech/simu-scs) by Toyota InfoTech
- **Fork Maintainer**: [Giuseppe Balzano](https://github.com/Balzakrez)
- **Built on**: INET, os3, leosatellites, Veins, Simu5G

---

## Related Projects

- **[INET Framework](https://github.com/inet-framework/inet)** - Foundation network simulation framework providing protocol implementations, physical layer models, and mobility support for OMNeT++
- **[os3](https://github.com/inet-framework/os3)** - Satellite orbit simulation library providing SGP4 orbital mechanics and NORAD TLE support for LEO satellite modeling
- **[leosatellites](https://github.com/Avian688/leosatellites)** - LEO satellite constellation framework with propagation models, network configurators, and satellite-specific physical layer implementations
- **[Veins](https://github.com/sommer/veins)** - Vehicular network simulation framework with SUMO traffic simulator integration via TraCI protocol for realistic vehicle mobility
- **[Simu5G](https://github.com/Unipisa/Simu5G)** - 5G NR/LTE network simulator providing cellular protocol stack, radio resource management, and end-to-end network simulation capabilities