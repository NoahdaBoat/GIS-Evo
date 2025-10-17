#ifndef STREETSDATABASEAPI_H
#define STREETSDATABASEAPI_H

#include <string>
#include <vector>
#include "LatLon.h"

// Type definitions
using OSMID = unsigned long long;
using IntersectionIdx = unsigned;
using POIIdx = unsigned;
using FeatureIdx = unsigned;
using StreetSegmentIdx = unsigned;
using StreetIdx = unsigned;

// FeatureType enumeration
enum class FeatureType {
    UNKNOWN,
    PARK,
    BEACH,
    LAKE,
    RIVER,
    ISLAND,
    BUILDING,
    GREENSPACE,
    GOLFCOURSE,
    STREAM,
    GLACIER
};

struct OSMNode {
    OSMID id() const { return 0; }
    LatLon coords() const { return LatLon(0, 0); }
};

struct OSMWay {
    OSMID id() const { return 0; }
    bool isClosed() const { return false; }
};

struct OSMRelation {
    OSMID id() const { return 0; }
};

// Missing enums for POI and features (stubbed for compilation)

// Struct definitions
struct StreetSegmentInfo {
    IntersectionIdx from;
    IntersectionIdx to;
    StreetIdx streetID;
    OSMID wayOSMID;
    unsigned numCurvePoints;
    bool oneWay;
    double speedLimit;
};

// Forward declarations - actual definitions are in struct.h and ms4helpers.hpp
struct Delivery_Stop;
using DeliveryInf = Delivery_Stop;

// CourierSubPath represents a portion of a delivery route
struct CourierSubPath {
    std::vector<StreetSegmentIdx> subpath;
    IntersectionIdx start_intersection;
    IntersectionIdx end_intersection;
    std::pair<IntersectionIdx, IntersectionIdx> intersections;

    CourierSubPath() : start_intersection(0), end_intersection(0) {}
};

struct OneRoute;

// Function declarations
bool load_map(std::string map_name);
void close_map();

unsigned getNumPointsOfInterest();
LatLon getPOIPosition(POIIdx poi_idx);
std::string getPOIType(POIIdx poi_idx);

unsigned getNumIntersections();
LatLon getIntersectionPosition(IntersectionIdx intersection_idx);
OSMID getIntersectionOSMNodeID(IntersectionIdx intersection_idx);
std::string getIntersectionName(IntersectionIdx intersection_idx);

unsigned getNumberOfNodes();
const OSMNode* getNodeByIndex(unsigned node_idx);
LatLon getNodeCoords(const OSMNode* node);
unsigned getTagCount(const OSMNode* node);
std::pair<std::string, std::string> getTagPair(const OSMNode* node, unsigned tag_idx);

unsigned getNumberOfPOIs();
std::string getPOIName(POIIdx poi_idx);
std::string getPOIType(POIIdx poi_idx);  // Keep original string version, remove POIType version to avoid ambiguity
LatLon getPOIPosition(POIIdx poi_idx);

unsigned getFeaturePointCount(FeatureIdx feature_idx);
LatLon getFeaturePoint(unsigned point_idx, FeatureIdx feature_idx);

unsigned getNumStreetSegments();
StreetSegmentInfo getStreetSegmentInfo(StreetSegmentIdx segment_idx);
unsigned getStreetSegmentCurvePointCount(StreetSegmentIdx segment_idx);
LatLon getStreetSegmentCurvePoint(unsigned point_idx, StreetSegmentIdx segment_idx);

std::string getStreetName(StreetIdx street_idx);
unsigned getNumStreets();
bool isClosedWay(const OSMWay* way);

// Intersection street segment functions
unsigned getNumIntersectionStreetSegment(IntersectionIdx intersection_idx);
unsigned getIntersectionStreetSegment(unsigned street_segment_num, IntersectionIdx intersection_idx);

// Feature functions
unsigned getNumFeatures();
FeatureType getFeatureType(FeatureIdx idx);
OSMID getFeatureOSMID(FeatureIdx idx);
std::string getFeatureName(FeatureIdx idx);
unsigned getNumFeaturePoints(FeatureIdx feature_idx);
LatLon getFeaturePoint(unsigned point_idx, FeatureIdx feature_idx);

// OSM functions
unsigned getNumberOfWays();
const OSMWay* getWayByIndex(unsigned way_idx);
unsigned getTagCount(const OSMWay* way);
std::pair<std::string, std::string> getTagPair(const OSMWay* way, unsigned tag_idx);

unsigned getNumberOfRelations();
const OSMRelation* getRelationByIndex(unsigned relation_idx);
unsigned getTagCount(const OSMRelation* relation);
std::pair<std::string, std::string> getTagPair(const OSMRelation* relation, unsigned tag_idx);
// Note: getRelationMembers is defined in OSMDatabaseAPI.h with TypedOSMID return type
std::vector<std::string> getRelationMemberRoles(const OSMRelation* relation);

// Stubbed POI accessors (from Task C5)

// Spatial query functions
std::vector<unsigned> queryStreetsInBounds(double min_x, double min_y, double max_x, double max_y);
std::vector<unsigned> queryIntersectionsInBounds(double min_x, double min_y, double max_x, double max_y);
std::vector<unsigned> queryPOIsInBounds(double min_x, double min_y, double max_x, double max_y);
std::vector<unsigned> queryFeaturesInBounds(double min_x, double min_y, double max_x, double max_y);

// Add more declarations as needed

// Missing functions for compilation
bool loadStreetsDatabaseBIN(std::string path);
void closeStreetDatabase();
const double NO_ANGLE = -1.0;

#endif // STREETSDATABASEAPI_H