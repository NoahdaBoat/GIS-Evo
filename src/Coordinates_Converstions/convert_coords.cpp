//
// Created by montinoa on 2/28/24.
//

#include "coords_conversions.hpp"
#include "LatLon.h"
#include "m1.h"
#include "../ms1helpers.h"

LatLon xy_to_latlon() {
    LatLon position;
    return position;
}

double lon_to_x(double longitude) {
    double value = kEarthRadiusInMeters * longitude * kDegreeToRadian * cos(globals.map_lat_avg);
    return value;
}

double lat_to_y(double latitude) {
    return kEarthRadiusInMeters * kDegreeToRadian * latitude;
}

double y_to_lat(double y) {
    return y/(kEarthRadiusInMeters * kDegreeToRadian);
}

double x_to_lon(double x) {
    return x/(kEarthRadiusInMeters * cos(globals.map_lat_avg) * kDegreeToRadian);
}

ezgl::point2d latlonTopoint(LatLon latlon){
    double x = lon_to_x(latlon.longitude());
    double y = lat_to_y(latlon.latitude());
    ezgl::point2d x_y_point2d(x, y);
    return x_y_point2d;
}