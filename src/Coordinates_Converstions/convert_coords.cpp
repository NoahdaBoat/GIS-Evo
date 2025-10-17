//
// Created by montinoa on 2/28/24.
//

#include "coords_conversions.hpp"
#include "LatLon.h"
#include <cmath>

LatLon xy_to_latlon() {
    LatLon position;
    return position;
}

double lon_to_x(double longitude, double map_lat_avg_rad) {
    double value = coords::kEarthRadiusInMeters * longitude * coords::kDegreeToRadian * cos(map_lat_avg_rad);
    return value;
}

double lat_to_y(double latitude) {
    return coords::kEarthRadiusInMeters * coords::kDegreeToRadian * latitude;
}

double y_to_lat(double y) {
    return y/(coords::kEarthRadiusInMeters * coords::kDegreeToRadian);
}

double x_to_lon(double x, double map_lat_avg_rad) {
    return x/(coords::kEarthRadiusInMeters * cos(map_lat_avg_rad) * coords::kDegreeToRadian);
}

Point2D latlonTopoint(LatLon latlon, double map_lat_avg_rad){
    double x = lon_to_x(latlon.longitude(), map_lat_avg_rad);
    double y = lat_to_y(latlon.latitude());
    Point2D x_y_point2d{x, y};
    return x_y_point2d;
}