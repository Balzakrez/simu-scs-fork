// 
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package scs_utils.converter;

//
// Position Converter Module
// 
// This module handles TWO coordinate conversion systems:
//
// 1. TOYOTA SYSTEM (GEO → PIXEL):
//    - For visualization of MCC, satellites, beams on OMNeT++ canvas
//    - Use: currentXposition(), currentYposition()
//    - Do not use for propagation calculations!
//
// 2. METERS ↔ GEO SYSTEM:
//    - For accurate network calculations, propagation, 3D distances
//    - Use: convertPosXToLongitude(), convertPosYToLatitude()
//    - This is the system to use for vehicles ↔ satellites
//
// AUTOMATIC CONFIGURATION:
// If you provide the 'sumoNetFile' parameter, the module automatically reads
// the geographic coordinates from the SUMO .net.xml file, eliminating the need
// to manually configure referenceLatitude/Longitude.
//
simple PositionConverter
{
    parameters:
        @display("i=block/network2");
        @class(Satellite::PositionConverter);
        
        // ========== SUMO INTEGRATION ==========
        // If set, automatically loads referenceLatitude/Longitude from SUMO file
        string sumoNetFile = default("");  // Path to SUMO .net.xml file (e.g., "../sumodir/mynet.net.xml")
        
        // ========== MANUAL GEOGRAPHIC PARAMETERS (Fallback) ==========
        // Used only if sumoNetFile is empty or loading fails
        // Represent the geographic center of the SUMO/Veins map
        double referenceLatitude = default(0.0);   // Center Lat (decimal degrees)
        double referenceLongitude = default(0.0);  // Center Lon (decimal degrees)
        
        // ========== OMNET++ MAP DIMENSIONS ==========
        // Must match SUMO map dimensions (convBoundary)
        int emapx = default(1080);  // OMNeT++ map width (meters)
        int emapy = default(1080);  // OMNeT++ map height (meters)
        
        // ========== TOYOTA SYSTEM (Visualization - Optional) ==========
        // Required only if you use the Toyota system to visualize MCC/satellites
        int offsetX = default(0);       // Display X offset (pixels)
        int mapx = default(2160);       // WorldMap width (pixels)
        int mapy = default(1080);       // WorldMap height (pixels)
        int offset_x = default(1770);   // Map2 X offset in WorldMap (pixels)
        int offset_y = default(181);    // Map2 Y offset in WorldMap (pixels)
        int mapx2 = default(255);       // Map2 width (pixels)
        int mapy2 = default(255);       // Map2 height (pixels)
}