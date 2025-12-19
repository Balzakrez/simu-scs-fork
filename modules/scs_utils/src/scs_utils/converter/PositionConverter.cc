// 
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "scs_utils/converter/PositionConverter.h"

#include <cctype>
#include <algorithm>

using namespace omnetpp;

namespace Satellite {

    Define_Module(PositionConverter);


    PositionConverter::PositionConverter() {
        referenceLatitude = 0.0;
        referenceLongitude = 0.0;
        centerVeinsX = 0.0;
        centerVeinsY = 0.0;
        metersPerDegreeLat = 0.0; 
        metersPerDegreeLon = 0.0;
        sumoNetFile = ""; 
    }


    PositionConverter::~PositionConverter() {}


    void PositionConverter::initialize(int stage) {
        
        // Call base class initialize
        cSimpleModule::initialize(stage);

        if (stage == INITSTAGE_LOCAL) {

            // Offset for WorldMap visualization (when used with Veins/SUMO)
            this->offsetX = par("offsetX");   

            // Original WorldMap dimensions (pixels)
            this->mapx = par("mapx");      
            this->mapy = par("mapy");    

            // Offset and dimensions of Map2 sub-area (pixels)
            this->offset_x = par("offset_x");  
            this->offset_y = par("offset_y"); 
            this->mapx2 = par("mapx2");
            this->mapy2 = par("mapy2");
            // Actual (Emap) OMNeT++ map dimensions (meters)
            this->emapx = par("emapx");
            this->emapy = par("emapy");
            
            // Calculate Toyota scale factors        
            this->scalex = (double)this->emapx / (double)this->mapx2 ;
            this->scaley = (double)this->emapy / (double)this->mapy2;

            EV_INFO << "PositionConverter initialized - Map: " << this->emapx << "x" << this->emapy 
                    << "m, Scale: (" << this->scalex << ", " << this->scaley << ")" << std::endl;

            // ========== METERS <-> GEO SYSTEM ==========
            this->centerVeinsX = this->emapx / 2.0; 
            this->centerVeinsY = this->emapy / 2.0;

            EV_DETAIL << "Map center: (" << this->centerVeinsX << ", " << this->centerVeinsY << ")" << std::endl;

            // Try to load from SUMO file
            this->sumoNetFile = par("sumoNetFile").stdstringValue();

            if(!this->sumoNetFile.empty()) {
                EV_INFO << "Loading geographic bounds from SUMO net file: " << this->sumoNetFile << std::endl;
                loadSumoGeographicBounds();
            } 
            else {
                EV_INFO << "No SUMO net file specified, using manual parameters" << std::endl;
                this->referenceLatitude = par("referenceLatitude").doubleValue(); // Fallback manual parameters
                this->referenceLongitude = par("referenceLongitude").doubleValue(); // Fallback manual parameters
                
                if (this->referenceLatitude == 0.0 && this->referenceLongitude == 0.0) {
                    EV_WARN << "Reference coordinates are (0.0, 0.0) - geographic conversions will be incorrect! "
                            << "Set sumoNetFile or manual coordinates." << std::endl;
                }
            }

            // Calculate conversion factors
            calculateConversionFactors();

            EV_INFO << "Geographic reference: (" << this->referenceLatitude << "°, " 
                    << this->referenceLongitude << "°), Conversion: " << this->metersPerDegreeLat 
                    << " m/°Lat, " << this->metersPerDegreeLon << " m/°Lon" << std::endl;
        }
    }


    void PositionConverter::calculateConversionFactors() {
        // Latitude: approximately constant everywhere ~111 km per degree
        this->metersPerDegreeLat = 111000.0;

        // Longitude: depends on latitude
        double latRad = this->referenceLatitude * (M_PI / 180.0); // convert degrees to radians
        // Formula: 111320 * cos(latitude_rad)
        // 111320 is an approximate average of Earth's circumference divided by 360
        this->metersPerDegreeLon = 111320.0 * cos(latRad);

        EV_TRACE << "metersPerDegreeLon = " << this->metersPerDegreeLon << " at lat " << this->referenceLatitude << "°" << std::endl;
    }


    void PositionConverter::loadSumoGeographicBounds() {
        try {
            // Load the XML file
            cXMLElement* netXML = getEnvir()->getXMLDocument(sumoNetFile.c_str());
            
            if (netXML == nullptr) {
                throw cRuntimeError("Cannot load SUMO net file: %s", sumoNetFile.c_str());
            }
            
            EV_DETAIL << "SUMO XML loaded, parsing <location> tag..." << std::endl;
            
            // Find the <location> tag
            cXMLElement* locationTag = netXML->getFirstChildWithTag("location");
            
            if (locationTag == nullptr) {
                throw cRuntimeError("No <location> tag found in SUMO net file");
            }
            
            // Read origBoundary
            const char* origBoundaryStr = locationTag->getAttribute("origBoundary");
            if (origBoundaryStr == nullptr) {
                throw cRuntimeError("No origBoundary attribute in <location> tag");
            }
            
            // Parse: "min_lon,min_lat,max_lon,max_lat"
            double minLon, minLat, maxLon, maxLat;
            if (sscanf(origBoundaryStr, "%lf,%lf,%lf,%lf", &minLon, &minLat, &maxLon, &maxLat) != 4) {
                throw cRuntimeError("Cannot parse origBoundary: %s", origBoundaryStr);
            }
            
            // Calculate the geographic center
            this->referenceLongitude = (minLon + maxLon) / 2.0;
            this->referenceLatitude = (minLat + maxLat) / 2.0;
            
            double latExtent = maxLat - minLat;
            double lonExtent = maxLon - minLon;
            
            EV_INFO << "SUMO bounds: [" << minLon << "°, " << minLat << "°] to [" << maxLon << "°, " << maxLat 
                    << "°], Center: (" << this->referenceLatitude << "°, " << this->referenceLongitude 
                    << "°), Extent: " << lonExtent << "° x " << latExtent << "°" << std::endl;
            
            // Read convBoundary to verify SUMO map dimensions
            const char* convBoundaryStr = locationTag->getAttribute("convBoundary");
            if (convBoundaryStr != nullptr) {
                double minX, minY, maxX, maxY;
                if (sscanf(convBoundaryStr, "%lf,%lf,%lf,%lf", &minX, &minY, &maxX, &maxY) == 4) {
                    double mapWidth = maxX - minX;
                    double mapHeight = maxY - minY;
                    
                    EV_DETAIL << "SUMO convBoundary: " << mapWidth << "m x " << mapHeight << "m" << std::endl;
                    
                    double widthDiff = fabs(mapWidth - this->emapx);
                    double heightDiff = fabs(mapHeight - this->emapy);
                    
                    if (widthDiff > 1.0 || heightDiff > 1.0) {
                        EV_WARN << "Map dimension mismatch detected: current " << this->emapx << "x" << this->emapy 
                                << " vs SUMO " << (int)mapWidth << "x" << (int)mapHeight 
                                << " - AUTO-UPDATING to match SUMO" << std::endl;
                        
                        this->emapx = (int)mapWidth;
                        this->emapy = (int)mapHeight;
                        
                        // Ricalcola centro
                        this->centerVeinsX = this->emapx / 2.0;
                        this->centerVeinsY = this->emapy / 2.0;
                        
                        // Ricalcola scale factors per sistema Toyota
                        this->scalex = (double)this->emapx / (double)this->mapx2;
                        this->scaley = (double)this->emapy / (double)this->mapy2;
                        
                        EV_INFO << "Updated map parameters: " << this->emapx << "x" << this->emapy 
                                << ", center (" << this->centerVeinsX << ", " << this->centerVeinsY 
                                << "), scale (" << this->scalex << ", " << this->scaley << ")" << std::endl;
                    } else {
                        EV_DETAIL << "Map dimensions consistent with SUMO" << std::endl;
                    }
                }
            }
            
        } catch (cRuntimeError& e) {
            EV_ERROR << "Failed loading SUMO bounds: " << e.what() << " - using manual parameters" << std::endl;
            
            // Fallback to manual parameters
            this->referenceLatitude = par("referenceLatitude").doubleValue();
            this->referenceLongitude = par("referenceLongitude").doubleValue();
            
            if (this->referenceLatitude == 0.0 && this->referenceLongitude == 0.0) {
                EV_WARN << "Manual parameters are (0.0, 0.0) - geographic conversions will fail!" << std::endl;
            } else {
                EV_INFO << "Using manual reference: (" << this->referenceLatitude << "°, " 
                        << this->referenceLongitude << "°)" << std::endl;
            }
        }
    }


    double PositionConverter::convertPosXToLongitude(float xPos) {
        // Calculate displacement from center in meters
        double deltaMeters = xPos - this->centerVeinsX;

        // Convert meters -> degrees using the factor calculated during initialization metersPerDegreeLon
        double deltaDegrees = deltaMeters / this->metersPerDegreeLon;

        // Result = Center + Variation
        double longitude = this->referenceLongitude + deltaDegrees;

        return longitude;
    }


    double PositionConverter::convertPosYToLatitude(float yPos) {
        // ATTENTION: Y grows DOWNWARD in OMNeT++
        // but Latitude grows UPWARD (North), so we invert the sign (yPos - centerVeinsY) -> (centerVeinsY - yPos)
        double deltaMeters = this->centerVeinsY - yPos;

        // Convert meters -> degrees
        double deltaDegrees = deltaMeters / this->metersPerDegreeLat;

        // Result = Center + Variation
        double latitude = this->referenceLatitude + deltaDegrees;
        
        return latitude;
    }

    
    float PositionConverter::currentXposition(double longitude) {
    
        // Normalize (longitude/360 + 1/2) for normalization from [-180, +180] to interval [0,1]
        // Note: mapx * (longitude/360) + mapx/2 ---> mapx * (longitude/360 + 1/2)
        float currentx = scalex * (mapx * (longitude/360) + mapx/2);

        // scale window offset
        currentx = currentx - scalex * offset_x;

        // Wrap-around or clamp to emapx
        if (emapx == mapx){
            currentx = static_cast<int>(currentx) % static_cast<int>(mapx);
        }
        else if (currentx > emapx)
        {
            currentx = emapx;
        }
        else if (currentx < 0)
        {
            currentx = 0;
        }

        // Add OffsetX for WorldMap visualization
        currentx = currentx + offsetX;
        currentX = currentx;

        return currentx;
    }


    float PositionConverter::currentYposition(double latitude)
    {
        float currenty = scaley * (-1 * mapy * (latitude / 180) + mapy / 2);

        currenty = currenty - scaley * offset_y;

        currentY = currenty;

        return currenty;
    }
    

}
