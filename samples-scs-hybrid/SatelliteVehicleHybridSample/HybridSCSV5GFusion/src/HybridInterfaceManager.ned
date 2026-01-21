
//
// HybridInterfaceManager.ned - Manages hard switching between satellite and cellular interfaces.
// 

package SatelliteVehicleHybridSample.HybridSCSV5GFusion.src;

simple HybridInterfaceManager
{
    parameters:
        @display("i=block/control");
        
        // ===== Interface Configuration =====
        string interfaceTableModule = default("^.interfaceTable");
        string satInterfaceName = default("wlan0");
        string cellInterfaceName = default("cellular");
        bool startWithSatellite = default(false);
        
        // ===== Switching Strategy =====
        string switchingMode @enum("time-based", "coverage-based", "energy-based");
        
        // ===== Time-Based Strategy Parameters =====
        double switchInterval @unit(s) = default(60s);

        // ===== Coverage-Based Strategy Parameters =====
        double coverageCheckInterval @unit(s) = default(1s);
        string satelliteModulePath = default("satellite[0]"); 

        // ===== Energy-Based Strategy Parameters =====
        double criticalEnergyThreshold @unit(J) = default(30J);
        double lowEnergyThreshold @unit(J) = default(90J);
        double highEnergyThreshold @unit(J) = default(240J);
        double energyCheckInterval @unit(s) = default(5s);

        // ===== Statistics =====
        
        // --- Common Switching Signals ---
        @signal[interfaceSignal](type=int);
        @statistic[activeInterface](
            source=interfaceSignal;
            title="Last Interface (0=Cellular, 1=Satellite)"; 
            record=last;
            interpolationmode=sample-hold;
        );
        
        @signal[switchSignal](type=long);
        @statistic[switchCount](
            source=switchSignal; 
            title="Total Switch Count"; 
            record=last;
        );
        
    
        // --- Interface Usage Metrics ---
        @signal[satUsageSignal](type=double);
        @statistic[satUsageTime](
            source=satUsageSignal;
            title="Satellite Usage Time"; 
            unit=s; 
            record=last
        );
        
        @signal[cellUsageSignal](type=double);
        @statistic[cellUsageTime](
            source=cellUsageSignal;
            title="Cellular Usage Time"; 
            unit=s; 
            record=last
        );

        // --- Coverage-Based Strategy Signals ---
        @signal[satelliteElevationSignal](type=double);
        @statistic[satelliteElevationDegree](
            source=satelliteElevationSignal;
            title="Satellite Elevation Angle"; 
            unit=deg; 
            record=vector,stats;
        );
        
        @signal[coverageLossCountSignal](type=int);
        @statistic[coverageLossCount](
            source=coverageLossCountSignal;
            title="Coverage Loss Count"; 
            record=last;
        );

        // --- Energy-Based Strategy Signals ---
        @signal[residualEnergySignal](type=double);
        @statistic[residualEnergy](
            source=residualEnergySignal;
            title="Residual Energy"; 
            unit=J; 
            record=vector,stats,last;
        );
        
}