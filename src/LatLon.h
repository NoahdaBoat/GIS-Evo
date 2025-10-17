#pragma once

#include <cmath>

struct LatLon {
    double lat;
    double lon;
    LatLon() : lat(0.0), lon(0.0) {}
    LatLon(double latitude, double longitude) : lat(latitude), lon(longitude) {}
    double latitude() const { return lat; }
    double longitude() const { return lon; }
    
    // Comparison operator
    bool operator==(const LatLon& other) const {
        return std::abs(lat - other.lat) < 1e-9 && std::abs(lon - other.lon) < 1e-9;
    }
};