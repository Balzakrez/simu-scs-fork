//
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef _COMMON_CONVERTER_POSITIONCONVERTER_H_
#define _COMMON_CONVERTER_POSITIONCONVERTER_H_

#include <omnetpp.h>
#include "inet/common/geometry/common/Coord.h"

using namespace omnetpp;
using namespace inet;

namespace Satellite {

/**
 * @brief Position converter module for coordinate transformations
 * 
 * This module handles conversions between:
 * - SUMO/Veins coordinates (meters)
 * - Geographic coordinates (latitude/longitude)
 * - Visualization coordinates (pixels)
 * 
 * Supports two visualization modes:
 * - Universal Mode: Full world map 1:1 display
 * - Legacy Mode: Cropped/zoomed Toyota system
 */
class PositionConverter : public cSimpleModule {
    
protected:
    
    std::string sumoNetFile; // Path to SUMO .net.xml file (optional)
    bool universalMode = false;   // Visualization mode flag: true=Universal, false=Legacy
    
    // --- Visualization: World-Map (background image) ---
    double mapx;                 // World-Map width (pixels)
    double mapy;                 // World-Map height (pixels)
    double worldMapDisplayWidth; // Display width for World-Map (pixels)
    double offsetX;              // X offset for World-Map positioning (pixels)
    
    // --- Visualization: SUMO/Veins Map ---
    double emapx;               // SUMO/Veins map width (meters)
    double emapy;               // SUMO/Veins map height (meters)
    
    // --- Toyota Legacy System (Crop/Zoom Mode) ---
    double offset_x;            // X offset for Map2 crop area (pixels)
    double offset_y;            // Y offset for Map2 crop area (pixels)
    double mapx2;               // Map2 cropped width (pixels)
    double mapy2;               // Map2 cropped height (pixels)
    
    // --- Scale factors ---
    double scalex;              // X scale factor for visualization
    double scaley;              // Y scale factor for visualization
    
    // --- Geographic reference ---
    double referenceLatitude;   // Map center latitude (degrees)
    double referenceLongitude;  // Map center longitude (degrees)
    double centerVeinsX;        // SUMO/Veins map center X (meters)
    double centerVeinsY;        // SUMO/Veins map center Y (meters)
    
    // --- Conversion factors ---
    double metersPerDegreeLat;  // Meters per degree latitude (~111 km)
    double metersPerDegreeLon;  // Meters per degree longitude (latitude-dependent)
    
    // --- Current visualization positions (cache) ---
    double currentXVeins;       // Last computed X pixel position
    double currentYVeins;       // Last computed Y pixel position
    
    // ========================================================================
    
    /**
     * @brief Initialize the position converter
     * @param stage Initialization stage (LOCAL or PHYSICAL_LAYER)
     */
    virtual void initialize(int stage) override;
    
    /**
     * @brief Number of initialization stages required
     * @return Number of stages
     */
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    
    /**
     * @brief Initialize visualization parameters (Stage 1)
     * 
     * Loads all visualization-related parameters from .ini file
     * and calculates initial scale factors.
     */
    virtual void initializeVisualizationParameters();
    
    /**
     * @brief Initialize geographic reference (Stage 2)
     * 
     * Attempts to load geographic coordinates in priority order:
     * 1. SUMO .net.xml file
     * 2. Manual parameters from .ini
     * 3. TraCI connection
     */
    virtual void initializeGeographicReference();
    
    /**
     * @brief Log initialization summary
     * 
     * Outputs a complete summary of loaded parameters and computed values.
     */
    virtual void logInitializationSummary();
    
    /**
     * @brief Check if module is in Universal Mode
     * @return True if Universal Mode (offset_x == 0 && offset_y == 0)
     */
    virtual bool isUniversalMode() const;
    
    /**
     * @brief Calculate scale factors based on current mode
     * 
     * Universal Mode: scalex = worldMapDisplayWidth / mapx
     * Legacy Mode: scalex = emapx / mapx2
     */
    virtual void calculateScaleFactors();
    
    /**
     * @brief Try to load geographic data from SUMO file
     * @return True if successful, false otherwise
     */
    virtual bool tryLoadFromSumoFile();
    
    /**
     * @brief Try to load geographic data from manual parameters
     * @return True if successful, false otherwise
     */
    virtual bool tryLoadManualCoordinates();
    
    /**
     * @brief Try to load geographic data from TraCI
     * @return True if successful, false otherwise
     */
    virtual bool tryLoadFromTraCI();
    
    /**
     * @brief Load geographic bounds and dimensions from SUMO .net.xml
     * 
     * Extracts:
     * - origBoundary: geographic bounds (lat/lon)
     * - convBoundary: cartesian dimensions (meters)
     * 
     * Updates emapx, emapy, referenceLatitude, referenceLongitude
     */
    virtual void loadSumoGeographicBounds();
    
    /**
     * @brief Parse origBoundary attribute (geographic bounds)
     * @param locationTag Pointer to <location> XML element
     */
    virtual void parseOrigBoundary(cXMLElement* locationTag);
    
    /**
     * @brief Parse convBoundary attribute (cartesian dimensions)
     * @param locationTag Pointer to <location> XML element
     */
    virtual void parseConvBoundary(cXMLElement* locationTag);
    
    /**
     * @brief Update map dimensions and recalculate dependent values
     * @param newWidth New map width (meters)
     * @param newHeight New map height (meters)
     */
    virtual void updateMapDimensions(double newWidth, double newHeight);
    
    /**
     * @brief Initialize geographic reference from TraCI connection
     * 
     * Queries SUMO via TraCI for the geographic position (lat/lon)
     * of the map center coordinates.
     * 
     * @return True if successful, false otherwise
     */
    virtual bool initFromTraCI();
    
    /**
     * @brief Calculate meters-to-degrees conversion factors
     * 
     * Computes:
     * - metersPerDegreeLat: ~111 km (constant)
     * - metersPerDegreeLon: 111.32 * cos(latitude) km
     */
    virtual void calculateConversionFactors();
    
public:
    
    /**
     * @brief Constructor
     */
    PositionConverter();
    
    /**
     * @brief Destructor
     */
    virtual ~PositionConverter();
    
    // ========================================================================

    // --- Meters (SUMO/Veins) -> Geographic (Lat/Lon) ---
    
    /**
     * @brief Convert X position (meters) to longitude (degrees)
     * 
     * Use for network calculations, propagation, coverage analysis.
     * 
     * @param xPos X position in meters (OMNeT++ coordinate system)
     * @return Longitude in decimal degrees
     */
    virtual double convertPosXToLongitude(float xPos);
    
    /**
     * @brief Convert Y position (meters) to latitude (degrees)
     * 
     * Use for network calculations, propagation, coverage analysis.
     * Note: Y grows downward in OMNeT++ but latitude grows upward.
     * 
     * @param yPos Y position in meters (OMNeT++ coordinate system)
     * @return Latitude in decimal degrees
     */
    virtual double convertPosYToLatitude(float yPos);
    
    // --- Geographic (Lat/Lon) -> Pixels (Visualization) ---
    
    /**
     * @brief Convert longitude to X pixel position for visualization
     * 
     * Used by Toyota system to display satellites, ground stations,
     * and beams on the OMNeT++ canvas.
     * 
     * @param longitude Longitude in decimal degrees
     * @return X position in pixels
     */
    virtual float currentXposition(double longitude);
    
    /**
     * @brief Convert latitude to Y pixel position for visualization
     * 
     * Used by Toyota system to display satellites, ground stations,
     * and beams on the OMNeT++ canvas.
     * 
     * @param latitude Latitude in decimal degrees
     * @return Y position in pixels
     */
    virtual float currentYposition(double latitude);
    
    /**
     * @brief Get X scale factor
     * @return X scale factor
     */
    virtual double isScaleX() { return scalex; }
    
    /**
     * @brief Get Y scale factor
     * @return Y scale factor
     */
    virtual double isScaleY() { return scaley; }
    
    /**
     * @brief Get reference latitude (map center)
     * @return Reference latitude in decimal degrees
     */
    virtual double getReferenceLatitude() const { return referenceLatitude; }
    
    /**
     * @brief Get reference longitude (map center)
     * @return Reference longitude in decimal degrees
     */
    virtual double getReferenceLongitude() const { return referenceLongitude; }
};

} // namespace Satellite

#endif // _COMMON_CONVERTER_POSITIONCONVERTER_H_