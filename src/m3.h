#ifndef M3_H
#define M3_H

#include <vector>
#include <utility>
#include "LatLon.h"

// Type definitions
using IntersectionIdx = unsigned;
using StreetSegmentIdx = unsigned;

// ==================== Pathfinding Functions ====================

/**
 * Find the shortest travel-time path between two intersections
 * 
 * @param turn_penalty The time penalty (in seconds) for making a turn
 * @param intersect_ids A pair of intersection IDs (start, destination)
 * @return A vector of StreetSegmentIdx representing the path, or empty vector if no path exists
 */
std::vector<StreetSegmentIdx> findPathBetweenIntersections(
    double turn_penalty, 
    std::pair<IntersectionIdx, IntersectionIdx> intersect_ids
);

/**
 * Compute the total travel time for a given path
 * 
 * @param turn_penalty The time penalty (in seconds) for making a turn
 * @param path A vector of StreetSegmentIdx representing the path
 * @return The total travel time in seconds
 */
double computePathTravelTime(
    const double turn_penalty, 
    const std::vector<StreetSegmentIdx>& path
);

#endif // M3_H