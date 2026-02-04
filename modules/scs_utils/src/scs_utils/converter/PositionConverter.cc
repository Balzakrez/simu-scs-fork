//
// Copyright (C) 2023 TOYOTA MOTOR CORPORATION. ALL RIGHTS RESERVED.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "scs_utils/converter/PositionConverter.h"
#include "veins_scs/modules/mobility/traci/TraCICommandInterfaceScs.h"
#include "veins_scs/modules/mobility/traci/TraCIScenarioNetManager.h"
#include "veins/modules/mobility/traci/TraCIConnection.h"

using namespace omnetpp;

namespace Satellite {

Define_Module(PositionConverter);

PositionConverter::PositionConverter() {
    this->mapx = 0.0;
    this->mapy = 0.0;
    this->emapx = 0.0;
    this->emapy = 0.0;
    this->worldMapDisplayWidth = 0.0;
    this->offsetX = 0.0;
    this->offset_x = 0.0;
    this->offset_y = 0.0;
    this->mapx2 = 0.0;
    this->mapy2 = 0.0;
    this->referenceLatitude = 0.0;
    this->referenceLongitude = 0.0;
    this->centerVeinsX = 0.0;
    this->centerVeinsY = 0.0;
    this->metersPerDegreeLat = 0.0;
    this->metersPerDegreeLon = 0.0;
    this->sumoNetFile = "";
}

PositionConverter::~PositionConverter() {}

void PositionConverter::initialize(int stage) {
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        initializeVisualizationParameters();
        if (par("universalMode").isSet()) this->universalMode = par("universalMode").boolValue();
        else this->universalMode = false;
    }
    else if (stage == INITSTAGE_PHYSICAL_LAYER) {
        initializeGeographicReference();
        calculateConversionFactors();
        logInitializationSummary();
    }
}

void PositionConverter::initializeVisualizationParameters() {
    EV_INFO << "=== STAGE 1: Loading Visualization Parameters ===" << endl;
    // 1. Load World-Map dimensions (background image)
    this->mapx = par("mapx").doubleValue();
    this->mapy = par("mapy").doubleValue();
    // 2. Load WorldMap display width (for scaling)
    if (!par("worldMapDisplayWidth").isSet()) {
        throw cRuntimeError("Parameter worldMapDisplayWidth not set!");
    }
    this->worldMapDisplayWidth = par("worldMapDisplayWidth").doubleValue();
    // 3. Load SUMO/Veins map dimensions (will be updated from XML if available)
    this->emapx = par("emapx").doubleValue();
    this->emapy = par("emapy").doubleValue();
    // 4. Load Toyota Legacy parameters
    this->offset_x = par("offset_x").doubleValue();
    this->offset_y = par("offset_y").doubleValue();
    this->mapx2 = par("mapx2").doubleValue();
    this->mapy2 = par("mapy2").doubleValue();
    // 5. Load offsetX (for visualization positioning)
    if(!par("offsetX").isSet()) {
        throw cRuntimeError("Parameter offsetX not set!");
    }
    this->offsetX = par("offsetX").doubleValue();
    // 6. SUMO file path (will be used in Stage 2)
    this->sumoNetFile = par("sumoNetFile").stdstringValue();
    // 7. Calculate map center (will be recalculated if SUMO updates dimensions)
    this->centerVeinsX = this->emapx / 2.0;
    this->centerVeinsY = this->emapy / 2.0;
    // 8. Calculate initial scale factors
    calculateScaleFactors();
    EV_INFO << "Visualization Mode: " << (isUniversalMode() ? "UNIVERSAL" : "LEGACY (Crop/Zoom)") << endl;
    EV_INFO << "Map dimensions: " << this->emapx << "x" << this->emapy << " m" << endl;
    EV_INFO << "Scale factors: (" << this->scalex << ", " << this->scaley << ")" << endl;
}

void PositionConverter::initializeGeographicReference() {
    EV_INFO << "=== STAGE 2: Loading Geographic Reference ===" << endl;
    bool coordinatesFound = false;
    // Priority 1: Try loading from SUMO file
    if (!this->sumoNetFile.empty()) {
        coordinatesFound = tryLoadFromSumoFile();
    }
    // Priority 2: Fallback to manual parameters
    if (!coordinatesFound) {
        coordinatesFound = tryLoadManualCoordinates();
    }
    // Priority 3: Last resort - TraCI
    if (!coordinatesFound) {
        coordinatesFound = tryLoadFromTraCI();
    }
    //Validation
    if (!coordinatesFound && isUniversalMode()) {
        throw cRuntimeError("Geographic reference not found! Provide .net.xml, manual Lat/Lon, or ensure TraCI is connected.");
    }
    else if (coordinatesFound) {
        EV_INFO << "Geographic reference: (" << this->referenceLatitude << "°, " << this->referenceLongitude << "°)" << endl;
    }
    else {
        EV_WARN << "Geographic reference not found, but continuing in Legacy mode." << endl;
    }
}

bool PositionConverter::isUniversalMode() const {
    return this->universalMode;
}

void PositionConverter::calculateScaleFactors() {
    if (isUniversalMode()) {
        // Universal Mode: Scale to fit World-Map into display width
        this->scalex = this->worldMapDisplayWidth / this->mapx;
        this->scaley = this->scalex; // Maintain aspect ratio
    } 
    else {
        // Legacy Mode: Scale based on map2 crop area
        if (this->mapx2 > 0 && this->mapy2 > 0) {
            this->scalex = this->emapx / this->mapx2;
            this->scaley = this->emapy / this->mapy2;
        } 
        else {
            EV_WARN << "Invalid mapx2/mapy2 in Crop Mode! Using Universal scale." << endl;
            this->scalex = this->worldMapDisplayWidth / this->mapx;
            this->scaley = this->scalex;
        }
    }
}

bool PositionConverter::tryLoadFromSumoFile() {
    try {
        EV_INFO << "Attempting to load from SUMO file: " << this->sumoNetFile << endl;
        loadSumoGeographicBounds(); // Updates emapx, emapy, refLat, refLon
        EV_INFO << "Successfully loaded geographic data from SUMO file" << endl;
        return true;
    } 
    catch (cRuntimeError& e) {
        EV_WARN << "SUMO loading failed: " << e.what() << endl;
        return false;
    }
}

bool PositionConverter::tryLoadManualCoordinates() {
    this->referenceLatitude = par("referenceLatitude").doubleValue();
    this->referenceLongitude = par("referenceLongitude").doubleValue();
    if (this->referenceLatitude == 0.0 && this->referenceLongitude == 0.0) {
        EV_INFO << "Manual coordinates not set or are (0.0, 0.0)" << endl;
        return false;
    }
    EV_INFO << "Using manual geographic reference" << endl;
    return true;
}

bool PositionConverter::tryLoadFromTraCI() {
    EV_INFO << "Attempting to initialize from TraCI..." << endl;
    if (initFromTraCI()) {
        EV_INFO << "Successfully initialized from TraCI" << endl;
        return true;
    }
    EV_WARN << "TraCI initialization failed" << endl;
    return false;
}

void PositionConverter::logInitializationSummary() {
    EV_INFO << "=== INITIALIZATION COMPLETE ===" << endl;
    EV_INFO << "Mode: " << (isUniversalMode() ? "Universal" : "Legacy") << endl;
    EV_INFO << "Map: " << this->emapx << "x" << this->emapy << " m" << endl;
    EV_INFO << "Center: (" << this->centerVeinsX << ", " << this->centerVeinsY << ") m" << endl;
    EV_INFO << "Reference: (" << this->referenceLatitude << "°, " << this->referenceLongitude << "°)" << endl;
    EV_INFO << "Scale: (" << this->scalex << ", " << this->scaley << ")" << endl;
    EV_INFO << "Conversion: " << this->metersPerDegreeLat << " m/°Lat, " 
            << this->metersPerDegreeLon << " m/°Lon" << endl;
}

void PositionConverter::loadSumoGeographicBounds() {
    // Load and parse SUMO XML file
    cXMLElement* netXML = getEnvir()->getXMLDocument(sumoNetFile.c_str());
    if (netXML == nullptr) {
        throw cRuntimeError("Cannot load SUMO net file: %s", sumoNetFile.c_str());
    }
    cXMLElement* locationTag = netXML->getFirstChildWithTag("location");
    if (locationTag == nullptr) {
        throw cRuntimeError("No <location> tag found in SUMO net file");
    }
    // Parse geographic bounds (origBoundary)
    parseOrigBoundary(locationTag);
    // Parse and update map dimensions (convBoundary)
    parseConvBoundary(locationTag);
}

void PositionConverter::parseOrigBoundary(cXMLElement* locationTag) {
    const char* origBoundaryStr = locationTag->getAttribute("origBoundary");
    if (origBoundaryStr == nullptr) {
        throw cRuntimeError("No origBoundary attribute in <location> tag");
    }
    double minLon, minLat, maxLon, maxLat;
    if (sscanf(origBoundaryStr, "%lf,%lf,%lf,%lf", &minLon, &minLat, &maxLon, &maxLat) != 4) {
        throw cRuntimeError("Cannot parse origBoundary: %s", origBoundaryStr);
    }
    // Calculate geographic center
    this->referenceLongitude = (minLon + maxLon) / 2.0;
    this->referenceLatitude = (minLat + maxLat) / 2.0;
    EV_DETAIL << "SUMO bounds: [" << minLon << "°, " << minLat << "°] to [" 
              << maxLon << "°, " << maxLat << "°]" << endl;
    EV_DETAIL << "Center: (" << this->referenceLatitude << "°, " 
              << this->referenceLongitude << "°)" << endl;
}

void PositionConverter::parseConvBoundary(cXMLElement* locationTag) {
    const char* convBoundaryStr = locationTag->getAttribute("convBoundary");
    if (convBoundaryStr == nullptr) {
        EV_WARN << "No convBoundary found in SUMO file" << endl;
        return;
    }
    double minX, minY, maxX, maxY;
    if (sscanf(convBoundaryStr, "%lf,%lf,%lf,%lf", &minX, &minY, &maxX, &maxY) != 4) {
        EV_WARN << "Cannot parse convBoundary" << endl;
        return;
    }
    double sumoWidth = maxX - minX;
    double sumoHeight = maxY - minY;
    EV_INFO << "SUMO convBoundary: " << sumoWidth << "m x " << sumoHeight << "m" << endl;
    // Check if dimensions need updating
    if (fabs(sumoWidth - this->emapx) > 1.0 || fabs(sumoHeight - this->emapy) > 1.0) {
        EV_INFO << "Updating map dimensions from SUMO: " << sumoWidth << "x" << sumoHeight << " m" << endl;
        updateMapDimensions(sumoWidth, sumoHeight);
    } 
    else {
        EV_INFO << "Map dimensions consistent with SUMO" << endl;
    }
}

void PositionConverter::updateMapDimensions(double newWidth, double newHeight) {
    // Update dimensions
    this->emapx = newWidth;
    this->emapy = newHeight;
    // Recalculate center
    this->centerVeinsX = this->emapx / 2.0;
    this->centerVeinsY = this->emapy / 2.0;
    // Recalculate scale factors
    calculateScaleFactors();
    EV_INFO << "Updated: " << this->emapx << "x" << this->emapy << " m, "
            << "center (" << this->centerVeinsX << ", " << this->centerVeinsY << "), "
            << "scale (" << this->scalex << ", " << this->scaley << ")" << endl;
}

bool PositionConverter::initFromTraCI() {
    try {
        // Find TraCI manager module
        cModule* managerMod = getSimulation()->getSystemModule()->getSubmodule("manager");
        if (!managerMod) {
            // Fallback: search by type
            for (cModule::SubmoduleIterator it(getSimulation()->getSystemModule()); !it.end(); it++) {
                if (dynamic_cast<veins::TraCIScenarioNetManager*>(*it)) {
                    managerMod = *it;
                    break;
                }
            }
        }
        if (!managerMod) {
            EV_ERROR << "TraCIScenarioManager module not found!" << endl;
            return false;
        }
        // Get TraCI command interface
        auto* manager = check_and_cast<veins::TraCIScenarioNetManager*>(managerMod);
        auto* traci = dynamic_cast<veins::TraCICommandInterfaceScs*>(manager->getCommandInterface());
        if (!traci) {
            EV_WARN << "Could not cast to TraCICommandInterfaceScs!" << endl;
            return false;
        }
        // Query geographic position of map center
        veins::Coord centerCoord(this->centerVeinsX, this->centerVeinsY);
        std::pair<double, double> geoPos = traci->getLonLat(centerCoord);
        // Update reference coordinates
        this->referenceLongitude = geoPos.first;
        this->referenceLatitude = geoPos.second;
        EV_INFO << "TraCI returned: Lat=" << this->referenceLatitude 
                << ", Lon=" << this->referenceLongitude << endl;
        return true;
    } 
    catch (std::exception& e) {
        EV_ERROR << "Error during TraCI init: " << e.what() << endl;
        return false;
    }
}

void PositionConverter::calculateConversionFactors() {
    // Latitude: approximately constant at ~111 km per degree
    this->metersPerDegreeLat = 111000.0;
    // Longitude: depends on latitude (cos correction)
    double latRad = this->referenceLatitude * (M_PI / 180.0);
    this->metersPerDegreeLon = 111320.0 * cos(latRad);
    EV_DETAIL << "Conversion factors: " << this->metersPerDegreeLat << " m/°Lat, "
              << this->metersPerDegreeLon << " m/°Lon at " 
              << this->referenceLatitude << "°" << endl;
}

double PositionConverter::convertPosXToLongitude(float xPos) {
    double deltaMeters = xPos - this->centerVeinsX;
    double deltaDegrees = deltaMeters / this->metersPerDegreeLon;
    return this->referenceLongitude + deltaDegrees;
}

double PositionConverter::convertPosYToLatitude(float yPos) {
    // Note: Y grows downward in OMNeT++, but Latitude grows upward (North)
    double deltaMeters = this->centerVeinsY - yPos;
    double deltaDegrees = deltaMeters / this->metersPerDegreeLat;
    return this->referenceLatitude + deltaDegrees;
}

float PositionConverter::currentXposition(double longitude) {
    double currentx;
    if (isUniversalMode()) {
        // Universal Mode: Equirectangular projection [-180,+180] -> [0, mapx]
        double normalizedX = (longitude + 180.0) / 360.0;
        double pixelX = normalizedX * this->mapx;
        currentx = (pixelX * this->scalex) + this->offsetX;
    } 
    else {
        // Legacy Mode: Toyota system with crop/zoom
        currentx = this->scalex * (this->mapx * (longitude / 360.0 + 0.5));
        currentx -= this->scalex * this->offset_x;
        // Clamp to bounds
        if (this->emapx == this->mapx) {
            currentx = static_cast<int>(currentx) % static_cast<int>(this->mapx);
        } 
        else {
            currentx = std::max(0.0, std::min(currentx, this->emapx));
        }
        currentx += this->offsetX;
    }
    this->currentXVeins = currentx;
    return static_cast<float>(currentx);
}

float PositionConverter::currentYposition(double latitude) {
    float currenty;
    if (isUniversalMode()) {
        // Universal Mode: Equirectangular projection [90,-90] -> [0, mapy]
        // Note: +90° (North) is at Y=0 (top), -90° (South) is at Y=mapy (bottom)
        double normalizedY = (90.0 - latitude) / 180.0;
        double pixelY = normalizedY * this->mapy;
        currenty = pixelY * this->scaley;
    } 
    else {
        // Legacy Mode: Toyota system
        currenty = this->scaley * (-this->mapy * (latitude / 180.0) + this->mapy / 2.0);
        currenty -= this->scaley * this->offset_y;
    }
    this->currentYVeins = currenty;
    return static_cast<float>(currenty);
}

} // namespace Satellite