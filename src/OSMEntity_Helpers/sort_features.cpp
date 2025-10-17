//
// Created by montinoa on 2/28/24.
//

#include <gtk/gtk.h>
#include <vector>
#include <unordered_map>
#include <fstream>
#include "typed_osmid_helper.hpp"
#include "m2_way_helpers.hpp"
#include "../Coordinates_Converstions/coords_conversions.hpp"
#include "../StreetsDatabaseAPI.h"
#include "../OSMDatabaseAPI.h"
#include "../gtk4_types.hpp"
#include "../binary_loader/binary_database.hpp"

#define MAP_STEPS 8

//std::vector<feature_data> sort_features() {
//    //std::ofstream myfile;
//    //myfile.open("torontofeatures.csv");
//    std::vector<feature_data> all_features;
//    all_features.resize(getNumFeatures());
//
//    for (FeatureIdx i = 0; i < getNumFeatures(); ++i) {
//        all_features[i].id = getFeatureOSMID(i);
//        all_features[i].feature_name = getFeatureName(i);
//        all_features[i].type = getFeatureType(i);
//        //myfile << all_features[i].id << "," << all_features[i].feature_name << "," << all_features[i].type << ",\n";
//    }
//    //myfile.close();
//    return all_features;
//}

void sort_features() {
    std::vector<feature_info> destructive_open;
    std::vector<feature_info> park, building, beach, glacier, golfcourse, greenspace, island, lake, river, stream, unknown;
    //const std::string&  getFeatureName(FeatureIdx featureIdx);
    
    // TODO: Replace with actual API calls once available
    // For now, we'll create dummy data to avoid compilation errors
    uint num_features = 0; // getNumFeatures();
    for (uint i = 0; i < num_features; ++i) {
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double min_x = std::numeric_limits<double>::max();
        feature_info info;
        info.type = FeatureType::UNKNOWN; // getFeatureType(i);
        info.id = TypedOSMID(); // getFeatureOSMID(i);
        info.feature_name = ""; // getFeatureName(i);
        int points = 0; // getNumFeaturePoints(i);

        // TODO: Replace with actual feature point data once available
        // For now, we'll create dummy polygon data to avoid compilation errors
        bool is_polygon = false; // getFeaturePoint(0, i) == getFeaturePoint(points-1, i);
        if (is_polygon) { // polygon
            for (size_t j = 0; j < static_cast<size_t>(points); ++j) {
                // LatLon node_pos = getFeaturePoint(j, i);
                // TODO: Replace with actual coordinate conversion
                double x_pos = 0.0; // lon_to_x(node_pos.longitude());
                double y_pos = 0.0; // lat_to_y(node_pos.latitude());
                Point2D current_point2d{x_pos, y_pos};
                info.points.push_back(current_point2d);
            }
            info.y_max = 0.0; // lat_to_y(max_x);
            info.y_min = 0.0; // lat_to_y(min_x);
            info.x_max = 0.0; // lon_to_x(max_y);
            info.x_min = 0.0; // lon_to_x(min_y);
            info.x_avg = 0.0; // (lon_to_x(max_x)+lon_to_x(min_x))/2;
            info.y_avg = 0.0; // ((lat_to_y(max_y))+(lat_to_y(min_y)))/2;

            switch (info.type) {
                case FeatureType::PARK :
                    info.mycolour = {184/255.0, 244/255.0, 204/255.0, 1.0};
                    info.dark_colour = {60/255.0, 104/255.0, 99/255.0, 1.0};
                    info.zoom_lod = 2;
                    park.push_back(info);
                    break;

                case FeatureType::BUILDING :
                    info.mycolour = {213/255.0, 216/255.0, 219/255.0, 1.0};
                    info.dark_colour = {72/255.0, 94/255.0, 115/255.0, 225/255.0};
                    info.zoom_lod = 7;
                    building.push_back(info);
                    break;

                case FeatureType::BEACH :
                    info.mycolour = {245/255.0, 236/255.0, 211/255.0, 1.0};
                    info.dark_colour = {102/255.0, 126/255.0, 137/255.0, 1.0};
                    info.zoom_lod = 3;
                    beach.push_back(info);
                    break;

                case FeatureType::GLACIER :
                    info.mycolour = {232/255.0, 232/255.0, 232/255.0, 1.0};
                    info.dark_colour = {112/255.0, 129/255.0, 147/255.0, 1.0};
                    info.zoom_lod = 2;
                    glacier.push_back(info);
                    break;

                case FeatureType::GOLFCOURSE :
                    info.mycolour = {96/255.0, 191/255.0, 138/255.0, 1.0};
                    info.dark_colour = {34/255.0, 82/255.0, 77/255.0, 1.0};
                    info.zoom_lod = 3;
                    golfcourse.push_back(info);
                    break;

                case FeatureType::GREENSPACE :
                    info.mycolour = {184/255.0, 244/255.0, 204/255.0, 1.0};
                    info.dark_colour = {60/255.0, 104/255.0, 99/255.0, 1.0};
                    info.zoom_lod = 0;
                    greenspace.push_back(info);
                    break;

                case FeatureType::ISLAND :
                    info.mycolour = {153/255.0, 228/255.0, 186/255.0, 1.0};
                    info.dark_colour = {44/255.0, 93/255.0, 88/255.0, 1.0};
                    info.zoom_lod = -1;
                    island.push_back(info);
                    break;

                case FeatureType::LAKE :
                    info.mycolour = {130/255.0, 216/255.0, 245/255.0, 1.0};
                    info.dark_colour = {2/255.0, 14/255.0, 28/255.0, 1.0};
                    info.zoom_lod = -5;
                    lake.push_back(info);
                    break;

                case FeatureType::RIVER :
                    info.mycolour = {130/255.0, 216/255.0, 245/255.0, 1.0};
                    info.dark_colour = {2/255.0, 14/255.0, 28/255.0, 1.0};
                    info.zoom_lod = -1;
                    river.push_back(info);
                    break;

                case FeatureType::STREAM :
                    info.mycolour = {130/255.0, 216/255.0, 245/255.0, 1.0};
                    info.dark_colour = {2/255.0, 14/255.0, 28/255.0, 1.0};
                    info.zoom_lod = 1;
                    stream.push_back(info);
                    break;

                case FeatureType::UNKNOWN :
                    info.mycolour = {232/255.0, 232/255.0, 232/255.0, 1.0};
                    info.dark_colour = {68/255.0, 81/255.0, 93/255.0, 1.0};
                    info.zoom_lod = 4;
                    unknown.push_back(info);
                    break;

                default:
                    info.mycolour = {232/255.0, 232/255.0, 232/255.0, 1.0};
                    info.dark_colour = {68/255.0, 81/255.0, 93/255.0, 1.0};
                    info.zoom_lod = 4;
                    unknown.push_back(info);
                    break;
            }
//
// Created by montinoa on 2/28/24.
//

#include <gtk/gtk.h>
#include <vector>
#include <unordered_map>
#include <fstream>
#include "typed_osmid_helper.hpp"
#include "m2_way_helpers.hpp"
#include "../Coordinates_Converstions/coords_conversions.hpp"
#include "../StreetsDatabaseAPI.h"
#include "../OSMDatabaseAPI.h"
#include "../gtk4_types.hpp"
#include "../binary_loader/binary_database.hpp"

#define MAP_STEPS 8

// Global variables for feature data
std::vector<feature_info> closed_features;
std::vector<feature_info> open_features;
std::vector<std::vector<feature_info>> spatial_hash;
std::vector<feature_info> always_draw;

//std::vector<feature_data> sort_features() {
//    //std::ofstream myfile;
//    //myfile.open("torontofeatures.csv");
//    std::vector<feature_data> all_features;
//    all_features.resize(getNumFeatures());
//
//    for (FeatureIdx i = 0; i < getNumFeatures(); ++i) {
//        all_features[i].id = getFeatureOSMID(i);
//        all_features[i].feature_name = getFeatureName(i);
//        all_features[i].type = getFeatureType(i);
//        //myfile << all_features[i].id << "," << all_features[i].feature_name << "," << all_features[i].type << ",\n";
//    }
//    //myfile.close();
//    return all_features;
//}

void sort_features() {
    std::vector<feature_info> destructive_open;
    std::vector<feature_info> park, building, beach, glacier, golfcourse, greenspace, island, lake, river, stream, unknown;
    
    // Get the BinaryDatabase instance for coordinate conversion
    auto& db = gisevo::BinaryDatabase::instance();
    double map_lat_avg_rad = db.get_avg_lat_rad();
    
    // TODO: Replace with actual API calls once available
    // For now, we'll create dummy data to avoid compilation errors
    uint num_features = 0; // getNumFeatures();
    for (uint i = 0; i < num_features; ++i) {
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double min_x = std::numeric_limits<double>::max();
        feature_info info;
        info.type = FeatureType::UNKNOWN; // getFeatureType(i);
        info.id = TypedOSMID(); // getFeatureOSMID(i);
        info.feature_name = ""; // getFeatureName(i);
        int points = 0; // getNumFeaturePoints(i);

        // TODO: Replace with actual feature point data once available
        // For now, we'll create dummy polygon data to avoid compilation errors
        bool is_polygon = false; // getFeaturePoint(0, i) == getFeaturePoint(points-1, i);
        if (is_polygon) { // polygon
            for (size_t j = 0; j < static_cast<size_t>(points); ++j) {
                // LatLon node_pos = getFeaturePoint(j, i);
                // TODO: Replace with actual coordinate conversion
                double x_pos = 0.0; // lon_to_x(node_pos.longitude());
                double y_pos = 0.0; // lat_to_y(node_pos.latitude());
                Point2D current_point2d{x_pos, y_pos};
                info.points.push_back(current_point2d);
            }
            info.y_max = 0.0; // lat_to_y(max_x);
            info.y_min = 0.0; // lat_to_y(min_x);
            info.x_max = 0.0; // lon_to_x(max_y);
            info.x_min = 0.0; // lon_to_x(min_y);
            info.x_avg = 0.0; // (lon_to_x(max_x)+lon_to_x(min_x))/2;
            info.y_avg = 0.0; // ((lat_to_y(max_y))+(lat_to_y(min_y)))/2;

            switch (info.type) {
                case FeatureType::PARK :
                    info.mycolour = {184/255.0, 244/255.0, 204/255.0, 1.0};
                    info.dark_colour = {60/255.0, 104/255.0, 99/255.0, 1.0};
                    info.zoom_lod = 2;
                    park.push_back(info);
                    break;

                case FeatureType::BUILDING :
                    info.mycolour = {213/255.0, 216/255.0, 219/255.0, 1.0};
                    info.dark_colour = {72/255.0, 94/255.0, 115/255.0, 225/255.0};
                    info.zoom_lod = 7;
                    building.push_back(info);
                    break;

                case FeatureType::BEACH :
                    info.mycolour = {245/255.0, 236/255.0, 211/255.0, 1.0};
                    info.dark_colour = {102/255.0, 126/255.0, 137/255.0, 1.0};
                    info.zoom_lod = 3;
                    beach.push_back(info);
                    break;

                case FeatureType::GLACIER :
                    info.mycolour = {232/255.0, 232/255.0, 232/255.0, 1.0};
                    info.dark_colour = {112/255.0, 129/255.0, 147/255.0, 1.0};
                    info.zoom_lod = 2;
                    glacier.push_back(info);
                    break;

                case FeatureType::GOLFCOURSE :
                    info.mycolour = {96/255.0, 191/255.0, 138/255.0, 1.0};
                    info.dark_colour = {34/255.0, 82/255.0, 77/255.0, 1.0};
                    info.zoom_lod = 3;
                    golfcourse.push_back(info);
                    break;

                case FeatureType::GREENSPACE :
                    info.mycolour = {184/255.0, 244/255.0, 204/255.0, 1.0};
                    info.dark_colour = {60/255.0, 104/255.0, 99/255.0, 1.0};
                    info.zoom_lod = 0;
                    greenspace.push_back(info);
                    break;

                case FeatureType::ISLAND :
                    info.mycolour = {153/255.0, 228/255.0, 186/255.0, 1.0};
                    info.dark_colour = {44/255.0, 93/255.0, 88/255.0, 1.0};
                    info.zoom_lod = -1;
                    island.push_back(info);
                    break;

                case FeatureType::LAKE :
                    info.mycolour = {130/255.0, 216/255.0, 245/255.0, 1.0};
                    info.dark_colour = {2/255.0, 14/255.0, 28/255.0, 1.0};
                    info.zoom_lod = -5;
                    lake.push_back(info);
                    break;

                case FeatureType::RIVER :
                    info.mycolour = {130/255.0, 216/255.0, 245/255.0, 1.0};
                    info.dark_colour = {2/255.0, 14/255.0, 28/255.0, 1.0};
                    info.zoom_lod = -1;
                    river.push_back(info);
                    break;

                case FeatureType::STREAM :
                    info.mycolour = {130/255.0, 216/255.0, 245/255.0, 1.0};
                    info.dark_colour = {2/255.0, 14/255.0, 28/255.0, 1.0};
                    info.zoom_lod = 1;
                    stream.push_back(info);
                    break;

                case FeatureType::UNKNOWN :
                    info.mycolour = {232/255.0, 232/255.0, 232/255.0, 1.0};
                    info.dark_colour = {68/255.0, 81/255.0, 93/255.0, 1.0};
                    info.zoom_lod = 4;
                    unknown.push_back(info);
                    break;

                default:
                    info.mycolour = {232/255.0, 232/255.0, 232/255.0, 1.0};
                    info.dark_colour = {68/255.0, 81/255.0, 93/255.0, 1.0};
                    info.zoom_lod = 4;
                    unknown.push_back(info);
                    break;
            }
        }
        else {
            // TODO: Replace with actual feature point data once available
            for (size_t j = 0; j < static_cast<size_t>(points); ++j) {
                // LatLon node_pos = getFeaturePoint(j, i);
                // TODO: Replace with actual coordinate conversion
                double x_pos = 0.0; // lon_to_x(node_pos.longitude());
                double y_pos = 0.0; // lat_to_y(node_pos.latitude());
                Point2D current_point2d{x_pos, y_pos};
                info.points.push_back(current_point2d);
            }
            open_features.push_back(info);
        }
    }
    closed_features.insert(closed_features.end(), glacier.begin(), glacier.end());
    closed_features.insert(closed_features.end(), lake.begin(), lake.end());
    closed_features.insert(closed_features.end(), island.begin(), island.end());
    closed_features.insert(closed_features.end(), beach.begin(), beach.end());
    closed_features.insert(closed_features.end(), river.begin(), river.end());
    closed_features.insert(closed_features.end(), stream.begin(), stream.end());
    closed_features.insert(closed_features.end(), greenspace.begin(), greenspace.end());
    closed_features.insert(closed_features.end(), park.begin(), park.end());
    closed_features.insert(closed_features.end(), golfcourse.begin(), golfcourse.end());
    closed_features.insert(closed_features.end(), unknown.begin(), unknown.end());
    closed_features.insert(closed_features.end(), building.begin(), building.end());
    //destructive_open = closed_features;
//    double map_min_x = abs(lat_to_y(globals.min_lat));
//    double map_min_y = (lon_to_x(globals.min_lon));
//    spatial_hash.resize(MAP_STEPS*MAP_STEPS);
//    for (uint i = 0; i < closed_features.size(); ++i) {
//        int x_pos = (static_cast<int>(abs(closed_features[i].x_avg-map_min_x))) % MAP_STEPS;
//        int y_pos = (static_cast<int>(abs(closed_features[i].y_avg-map_min_y))) % MAP_STEPS;
//        if (closed_features[i].type == (FeatureType::LAKE) || closed_features[i].type == FeatureType::ISLAND) {
//            always_draw.push_back(closed_features[i]);
//        }
//        else {
//            spatial_hash[x_pos+y_pos].push_back(closed_features[i]);
//        }
//
////        std::vector<feature_info>::iterator iter = destructive_open.begin() + i;
////        destructive_open.erase(iter);
//    }

//    double box_x_min = globals.min_lat;
//    double box_y_min = globals.min_lon;
//    double step_x = (globals.max_lat - box_x_min)/MAP_STEPS;
//    double step_y = (globals.max_lon - box_y_min)/MAP_STEPS;
//    double box_y_max = box_y_min + step_y;
//    double box_x_max = box_x_min + step_x;
//    spatial_hash.reserve(MAP_STEPS*MAP_STEPS);
//    for (uint i = 0; i < MAP_STEPS; ++i) {
//        for (uint k = 0; k < MAP_STEPS; ++k) {
//            for (uint j = 0; j < destructive_open.size(); ++j) {
//                if ((destructive_open[j].x_min <= box_x_max) || (destructive_open[j].y_min <= box_y_min)) {
//                    spatial_hash[i+k].push_back(destructive_open[j]);
//                    std::vector<feature_info>::iterator iter = destructive_open.begin() + j;
//                    destructive_open.erase(iter);
//                }
//            }
//            box_x_min = box_x_max;
//            box_x_max += step_x;
//        }
//        box_y_min = box_y_max;
//        box_y_max += step_y;
//    }
}

std::unordered_map<OSMID, feature_data*> map_features_to_ways(std::vector<feature_data>& all_features) {
    std::unordered_map<OSMID, feature_data*> wayid_to_feature;
    for (size_t i = 0; i < all_features.size(); ++i) {
        if (all_features[i].id.type() == TypedOSMID::Way) {
            feature_data *pFeatureData = &all_features[i];
            wayid_to_feature.insert({all_features[i].id.id(), pFeatureData});
        }
    }
    return wayid_to_feature;
}

// need to go from the osmid of a way to the correct struct for that way to get the feature info

bool compare_ids(OSMID& way_id, feature_data& s1) {
    return s1.id == way_id;
}


bool set_lod_feature(int zoom_level, FeatureType type) {
    bool to_draw = false;

    switch (type) {

        case FeatureType::PARK :
            if (zoom_level > 2) {
                to_draw = true;
            }
            break;

        case FeatureType::BUILDING :
            if (zoom_level > 6) {
                to_draw = true;
            }
            break;

        case FeatureType::BEACH :
            if (zoom_level > 2) {
                to_draw = true;
            }
            break;

        case FeatureType::GLACIER :
            if (zoom_level > 2) {
                to_draw = true;
            }
            break;

        case FeatureType::GOLFCOURSE :
            if (zoom_level > 3) {
                to_draw = true;
            }
            break;

        case FeatureType::GREENSPACE :
            if (zoom_level > -1) {
                to_draw = true;
            }
            break;

        case FeatureType::ISLAND :
            if (zoom_level > -1) {
                to_draw = true;
            }
            break;

        case FeatureType::LAKE :
            //42, 104, 134, 255
            if (zoom_level > -5) {
                to_draw = true;
            }
            break;

        case FeatureType::RIVER :
            if (zoom_level > -1) {
                to_draw = true;
            }
            break;

        case FeatureType::STREAM :
            if (zoom_level > 1) {
                to_draw = true;
            }
            break;

        case FeatureType::UNKNOWN :
            if (zoom_level > 4) {
                to_draw = true;
            }
            // 232, 232, 232, 255
            break;

        default:
            if (zoom_level > 2) {
                to_draw = true;
            }
            break;
    }

    return to_draw;
}
