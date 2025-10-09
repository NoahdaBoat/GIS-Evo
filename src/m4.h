#ifndef M4_H
#define M4_H

#include <vector>
#include <string>
#include "LatLon.h"

// Type definitions
using IntersectionIdx = unsigned;
using StreetSegmentIdx = unsigned;

// Forward declarations for delivery-related structures
// These are defined in StreetsDatabaseAPI.h and ms4helpers.hpp
struct DeliveryInf;
struct CourierSubPath;

// ==================== Traveling Courier Problem ====================

/**
 * Solve the traveling courier problem for package delivery
 * 
 * Given a list of deliveries (each with a pickup and dropoff intersection),
 * a list of depot locations, and truck constraints, find an efficient route
 * that picks up and drops off all packages while respecting:
 *   - Packages must be picked up before they can be dropped off
 *   - Truck capacity limits
 *   - Turn penalties at intersections
 * 
 * @param deliveries Vector of delivery requests, each with pickup and dropoff locations
 * @param depots Vector of depot intersection IDs where the courier can start/end
 * @param turn_penalty Time penalty (in seconds) for making a turn at an intersection
 * @param truck_capacity Maximum number of packages the truck can carry at once
 * @return Vector of CourierSubPath representing the complete route
 */
std::vector<CourierSubPath> travelingCourier(
    const std::vector<DeliveryInf>& deliveries,
    const std::vector<IntersectionIdx>& depots,
    const float turn_penalty,
    const float truck_capacity
);

#endif // M4_H