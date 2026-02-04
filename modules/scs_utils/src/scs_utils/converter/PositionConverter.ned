// 
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package scs_utils.converter;

//
// Position Converter Module
// 
// Handles coordinate transformations between:
// - SUMO/Veins coordinates (meters)
// - Geographic coordinates (latitude/longitude in degrees)
// - Visualization coordinates (pixels on OMNeT++ canvas)
//
// ============================================================================
// AUTOMATIC FEATURES (when using SUMO .net.xml file):
// ============================================================================
// 1. Map Dimensions: Automatically extracted from convBoundary
// 2. Geographic Center: Automatically calculated from origBoundary
// 3. Scale Factors: Automatically computed based on mode
//
// ============================================================================
// VISUALIZATION MODES:
// ============================================================================
// 
// UNIVERSAL MODE (Recommended):
//   - Shows full world map with 1:1 aspect ratio
//   - Set: universalMode = true and offset_x = 0, offset_y = 0
//   - Scale: Automatically set to fit worldMapDisplayWidth
//   - Use case: Global satellite coverage visualization
//
// LEGACY MODE (Toyota System):
//   - Shows cropped/zoomed region from world map
//   - Set: universalMode = false and offset_x != 0 and/or offset_y != 0
//   - Scale: Based on mapx2/mapy2 crop dimensions
//   - Use case: Backward compatibility with Toyota simulations
//
// ============================================================================
// PARAMETER PRIORITY (Geographic Reference):
// ============================================================================
// 1. SUMO File: If sumoNetFile is set and valid -> use origBoundary
// 2. Manual: If SUMO fails -> use referenceLatitude/referenceLongitude
// 3. TraCI: If both fail -> query SUMO via TraCI connection
//
// ============================================================================

simple PositionConverter
{
    parameters:
        @display("i=block/network2");
        @class(Satellite::PositionConverter);
        
        // ====================================================================
        // SUMO INTEGRATION (Primary Source)
        // ====================================================================
        
        // Path to SUMO .net.xml file
        // If set, automatically loads:
        //   - Map dimensions (emapx, emapy) from convBoundary
        //   - Geographic center (refLat, refLon) from origBoundary
        string sumoNetFile = default("");
        bool universalMode = default(false); // true=Universal, false=Legacy Toyota
        
        // ====================================================================
        // GEOGRAPHIC REFERENCE (Fallback - Manual)
        // ====================================================================
        
        // Used only if sumoNetFile is empty or loading fails
        // Set to the geographic center of your simulation area
        double referenceLatitude = default(0.0);   // Center latitude (degrees)
        double referenceLongitude = default(0.0);  // Center longitude (degrees)
        
        // ====================================================================
        // VISUALIZATION - LEFT PANEL (SUMO/Veins Map)
        // ====================================================================
        
        // SUMO/Veins simulation area dimensions
        // Default -1 means: "Auto-extract from SUMO file"
        // If no SUMO file, you MUST set these manually
        double emapx = default(-1);  // Map width in meters
        double emapy = default(-1);  // Map height in meters
        
        // ====================================================================
        // VISUALIZATION - RIGHT PANEL (World Map Background)
        // ====================================================================
        
        // Dimensions of the background world map IMAGE file (in pixels)
        // These should match your actual image file dimensions
        // Example: equirectangular_world_2160x1080.png
        double mapx = default(2160);  // Image width (pixels)
        double mapy = default(1080);  // Image height (pixels)
        
        // Display width for the world map visualization
        // This determines the scale factor in Universal Mode
        // Typical value: same as mapx for 1:1 display
        double worldMapDisplayWidth = default(2160);  // Display width (pixels)
        
        // Horizontal offset for world map positioning on canvas
        // In Universal Mode, this is typically set to 0
        // In Legacy Mode, this may be used to adjust positioning
        double offsetX = default(0);  // X offset (pixels)
        
        // ====================================================================
        // LEGACY TOYOTA SYSTEM (Crop/Zoom Mode)
        // ====================================================================
        
        // Set ALL to 0 for Universal Mode (full world map)
        // Set to non-zero values for Legacy Mode (cropped view)
        
        // For Universal Mode (RECOMMENDED):
        //   offset_x = 0
        //   offset_y = 0
        //   mapx2 = 0 (or any value, ignored)
        //   mapy2 = 0 (or any value, ignored)
        
        // For Legacy Mode (backward compatibility):
        //   These define a rectangular crop area from the world map
        //   Default values below are for Minato City, Tokyo, Japan
        
        double offset_x = default(0);   // Map2 X offset in world map (pixels)
        double offset_y = default(0);   // Map2 Y offset in world map (pixels)
        double mapx2 = default(0);      // Map2 crop width (pixels)
        double mapy2 = default(0);      // Map2 crop height (pixels)
        
        // Original Toyota defaults (for reference):
        // offset_x = 1770, offset_y = 181, mapx2 = 255, mapy2 = 255
}