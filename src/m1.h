#ifndef M1_H
#define M1_H

#include <string>
#include <vector>
#include <utility>
#include "LatLon.h"

// Type definitions
using IntersectionIdx = unsigned;
using POIIdx = unsigned;
using FeatureIdx = unsigned;
using StreetSegmentIdx = unsigned;
using StreetIdx = unsigned;
using OSMID = unsigned long long;

// Constants
const double kDegreeToRadian = 0.017453292519943295; // PI / 180
const double kEarthRadiusInMeters = 6371000.0;

// ==================== Map Loading/Closing ====================
bool loadMap(std::string map_streets_database_filename);
void closeMap();

// ==================== Distance/Travel Time Functions ====================
double findDistanceBetweenTwoPoints(LatLon point_1, LatLon point_2);
double findStreetSegmentLength(StreetSegmentIdx street_segment_id);
double findStreetSegmentTravelTime(StreetSegmentIdx street_segment_id);
double findStreetLength(StreetIdx street_id);
double findFeatureArea(FeatureIdx feature_id);
double findWayLength(OSMID way_id);

// ==================== Intersection Functions ====================
IntersectionIdx findClosestIntersection(LatLon my_position);
std::vector<IntersectionIdx> findIntersectionsOfStreet(StreetIdx street_id);
std::vector<IntersectionIdx> findIntersectionsOfTwoStreets(StreetIdx street_id1, StreetIdx street_id2);
std::vector<StreetSegmentIdx> findStreetSegmentsOfIntersection(IntersectionIdx intersection_id);
bool intersectionsAreDirectlyConnected(std::pair<IntersectionIdx, IntersectionIdx> intersection_ids);

// ==================== POI Functions ====================
POIIdx findClosestPOI(LatLon my_position, std::string poi_name);

// ==================== Street Functions ====================
std::vector<StreetIdx> findStreetIdsFromPartialStreetName(std::string street_prefix);

// ==================== OSM Functions ====================
std::string getOSMNodeTagValue(OSMID osm_id, std::string key);

#endif // M1_H