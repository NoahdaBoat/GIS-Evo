#pragma once

#include <vector>
#include <limits>
#include "LatLon.h"
#include "StreetsDatabaseAPI.h"

namespace gisevo {
    class BinaryDatabase;
}

// Type definitions (consistent with existing codebase)
using IntersectionIdx = unsigned;
using StreetSegmentIdx = unsigned;
using StreetIdx = unsigned;

// Structs for A* pathfinding
struct Search_Node {
    StreetSegmentIdx edge_id;
    double best_time;
    IntersectionIdx node_id;
    bool visited = false;
    
    Search_Node() : edge_id(0), best_time(std::numeric_limits<double>::max()), node_id(-1) {}
    Search_Node(StreetSegmentIdx e_id, double time, IntersectionIdx n_id)
        : edge_id(e_id), best_time(time), node_id(n_id) {}
};

struct Wave_Elm {
    IntersectionIdx node_id;
    StreetSegmentIdx edge_id;
    StreetIdx street_index;
    double travel_time;
    double heuristic;
    double total_cost;
    double turning_cost;

    Wave_Elm(IntersectionIdx n_id, StreetSegmentIdx e_id, StreetIdx s_idx, double t_time,
             double h = 0.0, double tc = 0.0, double turn_c = 0.0)
        : node_id(n_id), edge_id(e_id), street_index(s_idx), travel_time(t_time),
          heuristic(h), total_cost(tc), turning_cost(turn_c) {}
};

// Comparator for priority queue (min-heap based on total_cost)
struct comparator_dijkstra {
    bool operator()(const Wave_Elm& a, const Wave_Elm& b) const {
        return a.total_cost > b.total_cost;
    }
};

// Alias for compatibility
using comparator = comparator_dijkstra;

// ==================== Pathfinding Functions ====================

/**
 * Find the shortest travel-time path between two intersections using A* algorithm
 *
 * @param db Reference to the BinaryDatabase containing map data
 * @param start_id Starting intersection ID
 * @param end_id Destination intersection ID
 * @param turn_penalty The time penalty (in seconds) for making a turn
 * @return A vector of StreetSegmentIdx representing the path, or empty vector if no path exists
 */
std::vector<StreetSegmentIdx> findPathBetweenIntersections(
    const gisevo::BinaryDatabase& db,
    IntersectionIdx start_id,
    IntersectionIdx end_id,
    double turn_penalty
);

/**
 * Compute the total travel time for a given path
 *
 * @param db Reference to the BinaryDatabase containing map data
 * @param turn_penalty The time penalty (in seconds) for making a turn
 * @param path A vector of StreetSegmentIdx representing the path
 * @return The total travel time in seconds
 */
double computePathTravelTime(
    const gisevo::BinaryDatabase& db,
    const double turn_penalty,
    const std::vector<StreetSegmentIdx>& path
);

/**
 * A* algorithm implementation
 *
 * @param db Reference to the BinaryDatabase containing map data
 * @param start_id Starting intersection ID
 * @param end_id Destination intersection ID
 * @param turn_penalty The time penalty (in seconds) for making a turn
 * @return A vector of StreetSegmentIdx representing the path, or empty vector if no path exists
 */
std::vector<StreetSegmentIdx> aStarAlgorithm(
    const gisevo::BinaryDatabase& db,
    IntersectionIdx start_id,
    IntersectionIdx end_id,
    double turn_penalty
);

// ==================== Helper Functions ====================

/**
 * Get all street segments connected to an intersection
 */
std::vector<StreetSegmentIdx> findStreetSegmentsOfIntersection(const gisevo::BinaryDatabase& db, IntersectionIdx intersection_id);

/**
 * Get the travel time for a street segment (length / speed limit)
 */
double findStreetSegmentTravelTime(const gisevo::BinaryDatabase& db, StreetSegmentIdx segment_id);
