#pragma once

#include "courier_types.hpp"

namespace gisevo {
    class BinaryDatabase;
}

// ==================== Traveling Courier Problem ====================

/**
 * Solve the traveling courier problem for package delivery using BinaryDatabase
 * 
 * Given a list of deliveries (each with a pickup and dropoff intersection),
 * a list of depot locations, and truck constraints, find an efficient route
 * that picks up and drops off all packages while respecting:
 *   - Packages must be picked up before they can be dropped off
 *   - Truck capacity limits
 *   - Turn penalties at intersections
 * 
 * @param db Reference to the BinaryDatabase containing map data
 * @param deliveries Vector of delivery requests, each with pickup and dropoff locations
 * @param depots Vector of depot intersection IDs where the courier can start/end
 * @param turn_penalty Time penalty (in seconds) for making a turn at an intersection
 * @param truck_capacity Maximum number of packages the truck can carry at once
 * @return Vector of CourierSubPath representing the complete route
 */
std::vector<CourierSubPath> travelingCourier(
    const gisevo::BinaryDatabase& db,
    const std::vector<DeliveryInf>& deliveries,
    const std::vector<IntersectionIdx>& depots,
    const float turn_penalty,
    const float truck_capacity
);

// ==================== New BinaryDatabase-specific Functions ====================

/**
 * Compute travel times between all points of interest using parallel Dijkstra with BinaryDatabase
 */
void compute_all_travel_times(
    const gisevo::BinaryDatabase& db,
    const std::vector<IntersectionIdx>& of_interest,
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
    std::vector<std::vector<OneRoute>>& route_matrix,
    const float& turn_penalty
);

/**
 * Multi-source Dijkstra algorithm for computing routes from one intersection to many others using BinaryDatabase
 */
void multi_dijkstra(
    const gisevo::BinaryDatabase& db,
    IntersectionIdx start,
    const std::vector<IntersectionIdx>& of_interest,
    float turn_penalty,
    std::vector<std::vector<OneRoute>>& route_matrix,
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index
);

/**
 * Convert a path of intersection indices to CourierSubPath objects
 */
std::vector<CourierSubPath> indexToSubPath(
    const std::vector<IntersectionIdx>& path, 
    const std::vector<std::vector<OneRoute>>& routes_matrix, 
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index
);

/**
 * Simulated annealing optimization for route improvement using BinaryDatabase
 */
std::vector<IntersectionIdx> simulatedAnnealing(
    const gisevo::BinaryDatabase& db,
    int temperature,
    std::vector<IntersectionIdx> start_path,
    double start_path_cost,
    int num_perturbations,
    const std::vector<std::vector<OneRoute>>& routes_matrix,
    std::mt19937& rng,
    double alpha,
    double time_taken,
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index
);

/**
 * Generate a greedy initial solution for the traveling courier problem
 */
std::vector<IntersectionIdx> generateGreedySolution(
    const gisevo::BinaryDatabase& db,
    const std::vector<DeliveryInf>& deliveries,
    const std::vector<IntersectionIdx>& depots,
    const std::vector<std::vector<OneRoute>>& route_matrix,
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
    float truck_capacity
);
