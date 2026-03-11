
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
    
        string switchingMode @enum("time-based", "coverage-based", "energy-aware", "qos-based");
        
        // ===== Time-Based Strategy Parameters =====
        double switchInterval @unit(s) = default(60s);

        // ===== Coverage-Based Strategy Parameters =====
        double coverageCheckInterval @unit(s) = default(1s);

        // ===== QoS-Based Strategy Parameters =====
        double minAcceptablePDR = default(0.75); // minimum Packet Delivery Ratio (0.0 - 1.0)
        double minAcceptableThroughput @unit(bps) = default(0bps); // minimum throughput in bps
        double maxAcceptableJitter @unit(s) = default(0.05s); // max jitter in seconds
        double minQosScore = default(0.75); // Threshold for QoS degradation
        int minDegradationCount = default(3); // number of consecutive degradations before switching
        double qosCheckInterval @unit(s) = default(5s); // interval to check QoS in seconds
        double qosCutOffInterval @unit(s) = default(5s); // cutoff interval for delete old QoS samples
        double minHoldTime @unit(s) = default(10s); // cool-down time between switches in seconds
        // Weighted scoring for QoS-based strategy
        double weightRTT = default(0.4); // weight for RTT in decision making
        double weightPDR = default(0.3); // weight for PDR in decision making
        double weightJitter = default(0.2); // weight for Jitter in decision making

        // ===== Energy-Aware Strategy Parameters =====
        double criticalEnergyThreshold @unit(J) = default(30J);
        double energyCheckInterval @unit(s) = default(5s);
        double energyCutOffInterval @unit(s) = default(5s); // cutoff interval for delete old energy samples
        double satelliteEnergyCostPerByte @unit(J) = default(0.01J);
        double cellularEnergyCostPerByte @unit(J) = default(0.0002J);
        // Thresholds for energy-based strategy
        double maxAcceptableRTT @unit(s) = default(0.5s);
        double minUtilityScore = default(0.5);
        // Weighted scoring for energy-based strategy
        double weightEnergy = default(0.5);
        double weightQos = default(0.5);

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

        @signal[energyEfficiencySignal](type=double);
        @statistic[energyEfficiency](
            source=energyEfficiencySignal;
            title="Energy Efficiency Score"; 
            record=vector,stats,last;
        );

        @signal[satelliteEnergyConsumed](type=double);
        @statistic[satelliteEnergyConsumed](
            source=satelliteEnergyConsumed;
            title="Satellite Estimated Energy Consumed";
            unit=J;
            record=last;
        );

        @signal[cellularEnergyConsumed](type=double);
        @statistic[cellularEnergyConsumed](
            source=cellularEnergyConsumed;
            title="Cellular Estimated Energy Consumed";
            unit=J;
            record=last;
        );

        @signal[totalEnergyConsumed](type=double);
        @statistic[totalEnergyConsumed](
            source=totalEnergyConsumed;
            title="Total Estimated Energy Consumed";
            unit=J;
            record=last;
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
    
        @signal [qosScoreSignal](type=double);
        @statistic [qosScore](
            source=qosScoreSignal;
            title="QoS Score"; 
            record=vector,stats,last;
        );
        
        // ====================================================
        //  Cumulative Statistics Signals
        @signal[satelliteCumulativeThroughput](type=double);
        @statistic[satelliteCumulativeThroughput](
            source=satelliteCumulativeThroughput;
            title="Satellite Cumulative Throughput";
            unit=bps;
            record=last;
        );
        
        @signal[satelliteCumulativeRTT](type=double);
        @statistic[satelliteCumulativeRTT](
            source=satelliteCumulativeRTT;
            title="Satellite Cumulative Average RTT";
            unit=s;
            record=last;
        );
        
        @signal[satelliteCumulativePDR](type=double);
        @statistic[satelliteCumulativePDR](
            source=satelliteCumulativePDR;
            title="Satellite Cumulative Packet Delivery Ratio";
            record=last;
        );
        
        @signal[satelliteCumulativeJitter](type=double);
        @statistic[satelliteCumulativeJitter](
            source=satelliteCumulativeJitter;
            title="Satellite Cumulative Average Jitter";
            unit=s;
            record=last;
        );
        
        @signal[satelliteTotalBytes](type=double);
        @statistic[satelliteTotalBytes](
            source=satelliteTotalBytes;
            title="Satellite Total Bytes Transferred";
            unit=B;
            record=last;
        );
        
        // Cellular cumulative signals
        @signal[cellularCumulativeThroughput](type=double);
        @statistic[cellularCumulativeThroughput](
            source=cellularCumulativeThroughput;
            title="Cellular Cumulative Throughput";
            unit=bps;
            record=last;
        );
        
        @signal[cellularCumulativeRTT](type=double);
        @statistic[cellularCumulativeRTT](
            source=cellularCumulativeRTT;
            title="Cellular Cumulative Average RTT";
            unit=s;
            record=last;
        );
        
        @signal[cellularCumulativePDR](type=double);
        @statistic[cellularCumulativePDR](
            source=cellularCumulativePDR;
            title="Cellular Cumulative Packet Delivery Ratio";
            record=last;
        );
        
        @signal[cellularCumulativeJitter](type=double);
        @statistic[cellularCumulativeJitter](
            source=cellularCumulativeJitter;
            title="Cellular Cumulative Average Jitter";
            unit=s;
            record=last;
        );
        
        @signal[cellularTotalBytes](type=double);
        @statistic[cellularTotalBytes](
            source=cellularTotalBytes;
            title="Cellular Total Bytes Transferred";
            unit=B;
            record=last;
        );
        
        // Total across both interfaces
        @signal[totalBytesTransferred](type=double);
        @statistic[totalBytesTransferred](
            source=totalBytesTransferred;
            title="Total Bytes Transferred (All Interfaces)";
            unit=B;
            record=last;
        );
        // ====================================================
        
}