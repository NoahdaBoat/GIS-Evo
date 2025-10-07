// contains all the global variables, implemented helper functions and their purpose here
#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <unordered_map>
#include <string>
#include "m1.h"
#include "StreetsDatabaseAPI.h"
#include "OSMDatabaseAPI.h"
#include "struct.h"
#include "globals.h"

//Structure for the intersection_info


// class that holds all global variables

// instance of the Global_Var class


/* Converts a string to all lower cases
 * Called by: findStreetIdsFromPartialStreetName -> m1.cpp, loopAllStreets -> helper.cpp
 * Calls: None
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
void lowerCase(std::string& Orin_String);


/* Maps each OSMID with its corresponding OSMNode
 * Called by: loadMap -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
void mapOSMIDToNode();

/*
 *
 */
void mapOSMIDToWay();

/*
 *
 */
void mapOSMIDToRelation();

/* Calculates each OSMWay's distance
 * Called by: loadMap -> m1.cpp
 * Calls: findDistanceBetweenTwoPoints -> m1.cpp
 * Estimated Time Complexity: O(n^2), however the number of nodes per way is small, making it O(n)
 * Implemented in: helpers.cpp
 */
void preLoadWayDistance();


/* Finds and replaces part of a string
 * Called by: loadMap -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
void replaceString(std::string& currentStr, const std::string& toReplace, const std::string& replaceWith);


/* Calculates the average latitude given a vector of LatLon points
 * Called by: findFeatureArea -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
double latAverageOfFeature(std::vector<LatLon>& featureListOfLatLon);


/* Converts two points from latitude and longitude to rectangular coordinates given an average latitude
 * Called by: findFeatureArea -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(1)
 * Implemented in: helpers.cpp
 */
void convertLatLonToXY(LatLon point, double& x, double& y, double& avg);


/* Calculates area given 4 points. One piece of the trapezoid rule, the rest is in findFeatureArea
 * Called by: findFeatureArea -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(1)
 * Implemented in: helpers.cpp
 */
double trapezoidRule(double& x1, double& y1, double& x2, double& y2);


/* Loads the adjacent_intersections global variable such that the key is an IntersectionIdx and 
 * the value is a vector of IntersectionIdx of all directly connected intersections
 * Called by: loadMap -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
//void preLoadAjacentIntersections();


/* Loads intersections, street_segments and street_length of each StreetsInfo in vec_streetinfo
 * Loads each StreetSegementDistance in vec_segmentdis
 * Called by: loadMap -> m1.cpp
 * Calls: calculateSSLength -> helpers.cpp
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
void loopAllStreetSegments();


/* Calculates the length of the given street segment 
 * Called by: loopAllStreetSegments -> helpers.cpp
 * Calls: findDistanceBetweenTwoPoints
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
double CalculateSSLength(StreetSegmentIdx street_segment_id);


/* Loads intersection_street_segments with street segments connected at an intersection
 * organized by IntersectionIdx
 * Called by: loadMap -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
void preLoadIntersectionStreetSegment ();

 
/* Loads ordered_street_name with all street names in alphabetical order
 * and initializes a vector of streetsinfo 
 * Called by: loadMap -> m1.cpp
 * Calls: None
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
void loopAllStreets();


/* Implements nearly all the functionality required for findClosestPOI
 * Called by: findClosestPOI -> m1.cpp
 * Calls: findDistanceBetweenTwoPoints -> m1.cpp
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
POIIdx loopThroughAllPOIs(LatLon& my_position, std::string& poi_name);


/* Implements the loops required to calculate the area of a feature given its points
 * Called by: findFeatureArea -> m1.cpp
 * Calls: latAverageOfFeature -> helpers.cpp, convertLatLonToXY -> helpers.cpp, trapezoidRule -> helpers.cpp
 * Estimated Time Complexity: O(n)
 * Implemented in: helpers.cpp
 */
double getAreaFromFeaturePoints(int& num_of_feature_points, FeatureIdx& feature_id);


