
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
        string satelliteModulePath = default("satellite[0]"); 
        
        // ===== Switching Strategy =====
        string switchingMode @enum("time-based", "coverage-based", "energy-aware", "qos-based");
        
        // ===== Time-Based Strategy Parameters =====
        double switchInterval @unit(s) = default(60s);

        // ===== Coverage-Based Strategy Parameters =====
        double coverageCheckInterval @unit(s) = default(1s);

        // ===== Energy-Aware Strategy Parameters =====
        double criticalEnergyThreshold @unit(J) = default(30J);
        double lowEnergyThreshold @unit(J) = default(90J);
        double highEnergyThreshold @unit(J) = default(240J);
        double energyCheckInterval @unit(s) = default(5s);
        string optimizationMode @enum("minimize_energy", "maximize_qos", "balanced") = default("balanced");
        double satelliteEnergyCostPerByte @unit(J) = default(0.01J);
        double cellularEnergyCostPerByte @unit(J) = default(0.0002J);
        double minAcceptableRTT @unit(s) = default(0.01s);
        double maxAcceptableRTT @unit(s) = default(0.5s);

        // ===== QoS-Based Strategy Parameters =====
        double minAcceptablePDR = default(0.75); // minimum Packet Delivery Ratio (0.0 - 1.0)
        double minAcceptableThroughput @unit(bps) = default(0bps); // minimum throughput in bps
        double maxAcceptableDelay @unit(s) = default(0.5s); // max RTT in seconds
        double maxAcceptableJitter @unit(s) = default(0.05s); // max jitter in seconds
        double minQosScore = default(0.75); // Threshold for QoS degradation
        int minDegradationCount = default(3); // number of consecutive degradations before switching
        double qosCheckInterval @unit(s) = default(5s); // interval to check QoS in seconds
        double qosMeasurementWindow @unit(s) = default(5s); // time window for measurements in seconds
        double minHoldTime @unit(s) = default(10s); // cool-down time between switches in seconds
        // Weighted scoring
        double weightRTT = default(0.4); // weight for RTT in decision making
        double weightPDR = default(0.3); // weight for PDR in decision making
        double weightJitter = default(0.2); // weight for Jitter in decision making
        double weightThroughput = default(0.1); // weight for Throughput in decision making

        // ====================================================
        // General Signals (HybridInterfaceManager) 
        @signal[lastActiveInterfaceSignal](type=int);
        @statistic[lastActiveInterface](
            source=lastActiveInterfaceSignal;
            title="Last Active Interface (0=Cellular, 1=Satellite)"; 
            record=vector,stats,last;
        );
        
        @signal[switchCountSignal](type=long);
        @statistic[switchCount](
            source=switchCountSignal; 
            title="Total Switch Count"; 
            record=vector,stats,last;
        );

        @signal[satUsageSignal](type=double);
        @statistic[satUsageTime](
            source=satUsageSignal;
            title="Satellite Usage Time"; 
            unit=s; 
            record=vector,stats,last;
        );
        
        @signal[cellUsageSignal](type=double);
        @statistic[cellUsageTime](
            source=cellUsageSignal;
            title="Cellular Usage Time"; 
            unit=s; 
            record=vector,stats,last;
        );
        
        // ====================================================
        // Common Strategy Signals 
        @signal[currentRTTSignal](type=double);
        @statistic[currentRTT](
            source=currentRTTSignal;
            title="Current RTT"; 
            unit=s; 
            record=vector,stats,last;
        );

        @signal[currentPDRSignal](type=double);
        @statistic[currentPDR](
            source=currentPDRSignal;
            title="Current Packet Delivery Ratio"; 
            record=vector,stats,last;
        );

        // ====================================================
        // Coverage-Based Strategy Signals 
        @signal[satelliteElevationSignal](type=double);
        @statistic[satelliteElevationDegree](
            source=satelliteElevationSignal;
            title="Satellite Elevation Angle"; 
            unit=deg; 
            record=vector,stats,last;
        );
        
        @signal[coverageLossCountSignal](type=int);
        @statistic[coverageLossCount](
            source=coverageLossCountSignal;
            title="Coverage Loss Count"; 
            record=vector,stats,last;
        );

        // ====================================================
        // Energy-Based Strategy Signals 
        @signal[residualEnergySignal](type=double);
        @statistic[residualEnergy](
            source=residualEnergySignal;
            title="Residual Energy"; 
            unit=J; 
            record=vector,stats,last;
        );

        @signal[utilityScoreSignal](type=double);
        @statistic[utilityScore](
            source=utilityScoreSignal;
            title="Utility Score"; 
            record=vector,stats,last;
        );

        // ====================================================
        // QoS-Based Strategy Signals 
        @signal[currentJitterSignal](type=double);
        @statistic[currentJitter](
            source=currentJitterSignal;
            title="Current Jitter"; 
            unit=s; 
            record=vector,stats,last;
        );
    
        @signal [currentThroughputSignal](type=double);
        @statistic[currentThroughput](
            source=currentThroughputSignal;
            title="Current Throughput"; 
            unit=bps; 
            record=vector,stats,last;
        );
        
        @signal [qosScoreSignal](type=double);
        @statistic [qosScore](
            source=qosScoreSignal;
            title="QoS Score"; 
            record=vector,stats,last;
        );
        
        @signal [degradationCountSignal](type=long);
        @statistic [degradationCount](
            source=degradationCountSignal;
            title="Degradation Count"; 
            record=vector,stats,last;
        );
        
}