#pragma once

#include <vector>
#include <unordered_map>
#include <random>
#include "StreetsDatabaseAPI.h"

// Type definitions (consistent with existing codebase)
using IntersectionIdx = unsigned;
using StreetSegmentIdx = unsigned;
using StreetIdx = unsigned;

enum class Stop_Type{
    DEPOT = 0,
    PICKUP,
    DROPOFF,
};

struct OneRoute {
    std::vector<StreetSegmentIdx> route;
    double travel_time;
    IntersectionIdx start;
    IntersectionIdx end;

    // Constructors
    OneRoute(std::vector<StreetSegmentIdx> i_route,
             double i_travel_time,
             IntersectionIdx i_start,
             IntersectionIdx i_end)
        : route(std::move(i_route)), travel_time(i_travel_time), 
          start(i_start), end(i_end) {}
    
    OneRoute() : travel_time(0.0), start(0), end(0) {}
};

// Forward declarations for courier algorithms
std::vector<CourierSubPath> travelingCourier(
    const class gisevo::BinaryDatabase& db,
    const std::vector<DeliveryInf>& deliveries,
    const std::vector<IntersectionIdx>& depots,
    const double right_turn_penalty,
    const double left_turn_penalty,
    const double truck_capacity
);
