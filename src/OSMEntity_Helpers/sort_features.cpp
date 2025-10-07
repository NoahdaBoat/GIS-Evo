//
// Created by montinoa on 2/28/24.
//

#include <vector>
#include <unordered_map>
#include <fstream>
#include "typed_osmid_helper.hpp"
#include "m2_way_helpers.hpp"
#include "../Coordinates_Converstions/coords_conversions.hpp"
#include "/cad2/ece297s/public/include/streetsdatabase/StreetsDatabaseAPI.h"
#include "/cad2/ece297s/public/include/streetsdatabase/OSMDatabaseAPI.h"
#include "../ezgl/color.hpp"
#include "../ezgl/graphics.hpp"
#include "../globals.h"

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
    for (uint i = 0; i < getNumFeatures(); ++i) {
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double min_x = std::numeric_limits<double>::max();
        feature_info info;
        info.type = getFeatureType(i);
        info.id = getFeatureOSMID(i);
        info.feature_name = getFeatureName(i);
        int points = getNumFeaturePoints(i);

        if (getFeaturePoint(0, i) == getFeaturePoint(points-1, i)) { // polygon
            for (uint j = 0; j < points; ++j) {
                LatLon node_pos = getFeaturePoint(j, i);
                if (node_pos.latitude() > max_x) {
                    max_x = node_pos.latitude();
                }
                if (node_pos.latitude() < min_x) {
                    min_x = node_pos.latitude();
                }
                if (node_pos.longitude() > max_y) {
                    max_y = node_pos.longitude();
                }
                if (node_pos.longitude() < min_y) {
                    min_y = node_pos.longitude();
                }
                double x_pos = lon_to_x(node_pos.longitude());
                double y_pos = lat_to_y(node_pos.latitude());
                ezgl::point2d current_point2d = ezgl::point2d(x_pos, y_pos);
                info.points.push_back(current_point2d);
            }
            info.y_max = lat_to_y(max_x);
            info.y_min = lat_to_y(min_x);
            info.x_max = lon_to_x(max_y);
            info.x_min = lon_to_x(min_y);
            info.x_avg = (lon_to_x(max_x)+lon_to_x(min_x))/2;
            info.y_avg = ((lat_to_y(max_y))+(lat_to_y(min_y)))/2;

            switch (getFeatureType(i)) {
                case FeatureType::PARK :
                    info.mycolour = ezgl::color(184, 244, 204, 255);
                    info.dark_colour = ezgl::color(60, 104, 99, 255);
                    info.zoom_lod = 2;
                    park.push_back(info);
                    break;

                case FeatureType::BUILDING :
                    info.mycolour = ezgl::color(213, 216, 219, 255);
                    info.dark_colour = ezgl::color(72, 94, 115, 225);
                    info.zoom_lod = 7;
                    building.push_back(info);
                    break;

                case FeatureType::BEACH :
                    info.mycolour = ezgl::color(245, 236, 211, 255);
                    info.dark_colour = ezgl::color(102, 126, 137, 255);
                    info.zoom_lod = 3;
                    beach.push_back(info);
                    break;

                case FeatureType::GLACIER :
                    info.mycolour = ezgl::color(232, 232, 232, 255);
                    info.dark_colour = ezgl::color(112, 129, 147, 255);
                    info.zoom_lod = 2;
                    glacier.push_back(info);
                    break;

                case FeatureType::GOLFCOURSE :
                    info.mycolour = ezgl::color(96, 191, 138, 255);
                    info.dark_colour = ezgl::color(34, 82, 77, 255);
                    info.zoom_lod = 3;
                    golfcourse.push_back(info);
                    break;

                case FeatureType::GREENSPACE :
                    info.mycolour = ezgl::color(184, 244, 204, 255);
                    info.dark_colour = ezgl::color(60, 104, 99, 255);
                    info.zoom_lod = 0;
                    greenspace.push_back(info);
                    break;

                case FeatureType::ISLAND :
                    info.mycolour = ezgl::color(153, 228, 186, 255);
                    info.dark_colour = ezgl::color(44, 93, 88, 255);
                    info.zoom_lod = -1;
                    island.push_back(info);
                    break;

                case FeatureType::LAKE :
                    info.mycolour = ezgl::color(130, 216, 245, 255);
                    info.dark_colour = ezgl::color(2, 14, 28, 255);
                    info.zoom_lod = -5;
                    lake.push_back(info);
                    break;

                case FeatureType::RIVER :
                    info.mycolour = ezgl::color(130, 216, 245, 255);
                    info.dark_colour = ezgl::color(2, 14, 28, 255);
                    info.zoom_lod = -1;
                    river.push_back(info);
                    break;

                case FeatureType::STREAM :
                    info.mycolour = ezgl::color(130, 216, 245, 255);
                    info.dark_colour = ezgl::color(2, 14, 28, 255);
                    info.zoom_lod = 1;
                    stream.push_back(info);
                    break;

                case FeatureType::UNKNOWN :
                    info.mycolour = ezgl::color(232, 232, 232, 255);
                    info.dark_colour = ezgl::color(68, 81, 93, 255);
                    info.zoom_lod = 4;
                    unknown.push_back(info);
                    break;

                default:
                    info.mycolour = ezgl::color(232, 232, 232, 255);
                    info.dark_colour = ezgl::color(68, 81, 93, 255);
                    info.zoom_lod = 4;
                    unknown.push_back(info);
                    break;
            }
        }
        else {
            for (uint j = 0; j < points; ++j) {
                LatLon node_pos = getFeaturePoint(j, i);
                double x_pos = lon_to_x(node_pos.longitude());
                double y_pos = lat_to_y(node_pos.latitude());
                ezgl::point2d current_point2d = ezgl::point2d(x_pos, y_pos);
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
    for (int i = 0; i < all_features.size(); ++i) {
        if (all_features[i].id.type() == TypedOSMID::Way) {
            feature_data *pFeatureData = &all_features[i];
            wayid_to_feature.insert({all_features[i].id, pFeatureData});
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
