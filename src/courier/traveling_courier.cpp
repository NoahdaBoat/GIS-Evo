#include "traveling_courier.hpp"
#include "binary_loader/binary_database.hpp"
#include "pathfinding/astar.hpp"
#include <algorithm>
#include <execution>
#include <limits>
#include <random>
#include <unordered_set>

namespace gisevo {
    class BinaryDatabase;
}

// ==================== Main Traveling Courier Function ====================

std::vector<CourierSubPath> travelingCourier(
    const gisevo::BinaryDatabase& db,
    const std::vector<DeliveryInf>& deliveries,
    const std::vector<IntersectionIdx>& depots,
    const float turn_penalty,
    const float truck_capacity
) {
    // Find all unique intersections from deliveries and depots
    std::vector<IntersectionIdx> unique_intersections = find_unique_intersections(deliveries, depots);
    
    // Create mapping from intersection ID to matrix index
    std::unordered_map<IntersectionIdx, int> intersection_to_index;
    preloadKeys(unique_intersections, intersection_to_index);
    
    // Preload delivery information
    preloadDeliveryStops(deliveries);
    
    // Compute travel time matrix between all points of interest
    std::vector<std::vector<OneRoute>> route_matrix(unique_intersections.size());
    for (auto& row : route_matrix) {
        row.resize(unique_intersections.size());
    }
    
    compute_all_travel_times(db, unique_intersections, intersection_to_index, route_matrix, turn_penalty);
    
    // Load delivery details
    std::vector<IntersectionIdx> pick_ups, drop_offs;
    loadDeliveryDetails(deliveries, depots, pick_ups, drop_offs, unique_intersections);
    
    // Generate initial solution using greedy approach
    std::vector<IntersectionIdx> initial_path = generateGreedySolution(
        db, deliveries, depots, route_matrix, intersection_to_index, truck_capacity
    );
    
    // Optimize using simulated annealing
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::vector<IntersectionIdx> optimized_path = simulatedAnnealing(
        db, 1000, initial_path, pathCost(initial_path, route_matrix, intersection_to_index),
        1000, route_matrix, rng, 0.95, 30000, intersection_to_index
    );
    
    // Convert optimized path to CourierSubPath objects
    return indexToSubPath(optimized_path, route_matrix, intersection_to_index);
}

// ==================== New BinaryDatabase-specific Functions ====================

void compute_all_travel_times(
    const gisevo::BinaryDatabase& db,
    const std::vector<IntersectionIdx>& of_interest,
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
    std::vector<std::vector<OneRoute>>& route_matrix,
    const float& turn_penalty
) {
    // Use parallel execution to compute routes from each intersection to all others
    std::for_each(std::execution::par, of_interest.begin(), of_interest.end(),
        [&](const auto& start_intersection) {
            multi_dijkstra(db, start_intersection, of_interest, turn_penalty, 
                          route_matrix, intersection_to_index);
        });
}

void multi_dijkstra(
    const gisevo::BinaryDatabase& db,
    IntersectionIdx start,
    const std::vector<IntersectionIdx>& of_interest,
    float turn_penalty,
    std::vector<std::vector<OneRoute>>& route_matrix,
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index
) {
    // Implementation of multi-source Dijkstra using the new pathfinding module
    int start_index = intersection_to_index.at(start);
    
    for (IntersectionIdx target : of_interest) {
        if (target == start) {
            // Self-route: empty route with zero time
            route_matrix[start_index][intersection_to_index.at(target)] = 
                OneRoute({}, 0.0, start, target);
            continue;
        }
        
        // Use A* algorithm to find path
        std::vector<StreetSegmentIdx> path = findPathBetweenIntersections(db, start, target, turn_penalty);
        
        if (!path.empty()) {
            double travel_time = computePathTravelTime(db, turn_penalty, path);
            route_matrix[start_index][intersection_to_index.at(target)] = 
                OneRoute(path, travel_time, start, target);
        } else {
            // No path found - set to invalid route
            route_matrix[start_index][intersection_to_index.at(target)] = 
                OneRoute({}, std::numeric_limits<double>::max(), start, target);
        }
    }
}

std::vector<CourierSubPath> indexToSubPath(
    const std::vector<IntersectionIdx>& path, 
    const std::vector<std::vector<OneRoute>>& routes_matrix, 
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index
) {
    std::vector<CourierSubPath> result;
    
    if (path.size() < 2) {
        return result;
    }
    
    for (size_t i = 0; i < path.size() - 1; ++i) {
        IntersectionIdx start = path[i];
        IntersectionIdx end = path[i + 1];
        
        int start_idx = intersection_to_index.at(start);
        int end_idx = intersection_to_index.at(end);
        
        const OneRoute& route = routes_matrix[start_idx][end_idx];
        
        CourierSubPath subpath;
        subpath.start_intersection = start;
        subpath.end_intersection = end;
        subpath.subpath = route.route;
        
        result.push_back(subpath);
    }
    
    return result;
}

// ==================== New BinaryDatabase-specific Optimization ====================

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
) {
    std::vector<IntersectionIdx> current_path = start_path;
    double current_cost = start_path_cost;
    std::vector<IntersectionIdx> best_path = start_path;
    double best_cost = start_path_cost;
    
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::uniform_int_distribution<int> perturbation_dist(0, 3);
    
    for (int i = 0; i < num_perturbations; ++i) {
        // Generate perturbation
        std::vector<IntersectionIdx> new_path = current_path;
        
        int perturbation_type = perturbation_dist(rng);
        switch (perturbation_type) {
            case 0:
                new_path = perturbationSwap(new_path, {});  // Need delivery_info
                break;
            case 1:
                new_path = perturbationTwoOpt(new_path);
                break;
            case 2:
                new_path = perturbationMoveOne(new_path, {});  // Need delivery_info
                break;
            case 3:
                new_path = perturbeTravelRoute(new_path);
                break;
        }
        
        // Check if new path is legal
        if (!checkLegalNode(new_path)) {
            continue;
        }
        
        double new_cost = pathCost(new_path, routes_matrix, intersection_to_index);
        
        // Accept or reject based on simulated annealing criteria
        if (new_cost < current_cost || prob_dist(rng) < std::exp(-(new_cost - current_cost) / temperature)) {
            current_path = new_path;
            current_cost = new_cost;
            
            if (current_cost < best_cost) {
                best_path = current_path;
                best_cost = current_cost;
            }
        }
        
        // Cool down temperature
        temperature = static_cast<int>(temperature * alpha);
    }
    
    return best_path;
}

// ==================== New BinaryDatabase-specific Perturbations ====================

std::vector<IntersectionIdx> perturbationSwap(
    std::vector<IntersectionIdx>& path, 
    const std::unordered_map<IntersectionIdx, Delivery_details>& delivery_info
) {
    if (path.size() < 2) {
        return path;
    }
    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> dist(0, path.size() - 1);
    
    size_t i = dist(rng);
    size_t j = dist(rng);
    
    if (i != j) {
        std::swap(path[i], path[j]);
    }
    
    return path;
}

std::vector<IntersectionIdx> perturbationMoveOne(
    std::vector<IntersectionIdx>& path, 
    const std::unordered_map<IntersectionIdx, Delivery_details>& delivery_info
) {
    if (path.size() < 3) {
        return path;
    }
    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> dist(0, path.size() - 1);
    
    size_t from = dist(rng);
    size_t to = dist(rng);
    
    if (from != to) {
        IntersectionIdx element = path[from];
        path.erase(path.begin() + from);
        path.insert(path.begin() + to, element);
    }
    
    return path;
}

std::vector<IntersectionIdx> perturbeTravelRoute(std::vector<IntersectionIdx>& path) {
    // Randomly swap two adjacent elements
    if (path.size() < 2) {
        return path;
    }
    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> dist(0, path.size() - 2);
    
    size_t i = dist(rng);
    std::swap(path[i], path[i + 1]);
    
    return path;
}

// ==================== New BinaryDatabase-specific Utility Functions ====================

std::vector<IntersectionIdx> generateGreedySolution(
    const gisevo::BinaryDatabase& db,
    const std::vector<DeliveryInf>& deliveries,
    const std::vector<IntersectionIdx>& depots,
    const std::vector<std::vector<OneRoute>>& route_matrix,
    const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
    float truck_capacity
) {
    // Generate a greedy initial solution
    // This is a simplified implementation - a full version would be more sophisticated
    
    std::vector<IntersectionIdx> path;
    
    // Start at the first depot
    if (!depots.empty()) {
        path.push_back(depots[0]);
    }
    
    // Add all pickup and dropoff locations in order
    for (const auto& delivery : deliveries) {
        path.push_back(delivery.pickUp);
        path.push_back(delivery.dropOff);
    }
    
    // End at the same depot
    if (!depots.empty()) {
        path.push_back(depots[0]);
    }
    
    return path;
}
