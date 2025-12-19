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

    class PositionConverter : public cSimpleModule {

        protected:
            // ========== TOYOTA SYSTEM ==========
            double scalex;
            double scaley;

            float currentX;
            float currentY;

            int offsetX;    // X offset for WorldMap visualization (when used with Veins/SUMO)
        
            int mapx;       // Original WorldMap X dimension (pixels)
            int mapy;       // Original WorldMap Y dimension (pixels)

            int offset_x;   // X offset from WorldMap to delineate the sub-area (Map2)
            int offset_y;   // Y offset from WorldMap to delineate the sub-area (Map2)
            int mapx2;      // Extracted Map2 sub-area X dimension (pixels)
            int mapy2;      // Extracted Map2 sub-area Y dimension (pixels)
        
            int emapx;      // Actual (Emap) OMNeT++ map X dimension (meters)
            int emapy;      // Actual (Emap) OMNeT++ map Y dimension (meters)

            // ========== Geographic Parameters ==========
            double referenceLatitude;   // Map center Lat (degrees)
            double referenceLongitude;  // Map center Lon (degrees)
            double centerVeinsX;        // Veins map center X (meters)
            double centerVeinsY;        // Veins map center Y (meters)
            double metersPerDegreeLat;  // Conversion meters->degrees Lat
            double metersPerDegreeLon;  // Conversion meters->degrees Lon

            std::string sumoNetFile;    // Path to SUMO .net.xml file (if used)

            // ==============================================================================
            /**
             * Override initialize method from cSimpleModule.
             * Initializes the position converter by loading parameters and setting up
             * coordinate conversion systems.
             * @param stage Initialization stage number
             */
            virtual void initialize(int stage) override;
          
            /**
             * Calculates conversion factors for meters to degrees transformations.
             * Computes metersPerDegreeLat and metersPerDegreeLon based on the
             * reference latitude using Earth's geometry.
             */
            virtual void calculateConversionFactors();

            /**
             * Loads geographic bounds from SUMO .net.xml file.
             * Extracts origBoundary and calculates the geographic center of the map.
             * Extracts convBoundary and ensures consistency with emapx/emapy.
             * If loading fails, uses manual parameters as fallback.
             */
            virtual void loadSumoGeographicBounds();

        public: 
        
            /**
             * Constructor.
             * Initializes the PositionConverter module.
             */
            PositionConverter();
            
            /**
             * Destructor.
             * Cleans up the PositionConverter module resources.
             */
            virtual ~PositionConverter();

            // ==============================================================================
            // TOYOTA SYSTEM: GEO -> PIXEL: Methods used for visualizing GS, Beams and Satellites on the map
            
            /**
             * Converts longitude (in degrees) to X (pixel) position for visualization.
             * Uses the Toyota system for display on OMNeT++ canvas.
             * @param longitude Longitude in decimal degrees
             * @return X position in pixels
             */
            virtual float currentXposition(double longitude);
            
            /**
             * Converts latitude (in degrees) to Y (pixel) position for visualization.
             * Uses the Toyota system for display on OMNeT++ canvas.
             * @param latitude Latitude in decimal degrees
             * @return Y position in pixels
             */
            virtual float currentYposition(double latitude);

            /**
             * Gets the X scale factor.
             * @return X scale factor
             */
            virtual double isScaleX() { return scalex;};
            
            /**
             * Gets the Y scale factor.
             * @return Y scale factor
             */
            virtual double isScaleY() { return scaley;};

            // ==============================================================================
            // METERS <-> GEO SYSTEM: Methods to use for network calculations, coverage, distances, etc.
            /**
             * Converts X position (in meters) to longitude (in degrees).
             * Use this method for accurate network calculations and propagation.
             * @param xPos X position in meters (from OMNeT++ coordinate system)
             * @return Longitude in decimal degrees
             */
            virtual double convertPosXToLongitude(float xPos);
            
            /**
             * Converts Y position (in meters) to latitude (in degrees).
             * Use this method for accurate network calculations and propagation.
             * @param yPos Y position in meters (from OMNeT++ coordinate system)
             * @return Latitude in decimal degrees
             */
            virtual double convertPosYToLatitude(float yPos);

            // Getter/Setter Methods
            /**
             * Gets the reference latitude (map center).
             * @return Reference latitude in decimal degrees
             */
            virtual double getReferenceLatitude() const { return referenceLatitude; };
            
            /**
             * Gets the reference longitude (map center).
             * @return Reference longitude in decimal degrees
             */
            virtual double getReferenceLongitude() const { return referenceLongitude; }
            // ==============================================================================
        };
} // namespace Satellite
#endif
