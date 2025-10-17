#include "POI_setup.hpp"
#include "POI_helpers.hpp"
#include "../StreetsDatabaseAPI.h"
#include "../Coordinates_Converstions/coords_conversions.hpp"
#include "../binary_loader/binary_database.hpp"
#include <utility>
#include <string>

std::string poi_subordinates[]={"clinic", "police","library", "bank","college","public_building","condo","psychologist","food_bank","shelter","toilets"};
std::string poi_basics[]= {"hospital","emergency_room", "bus_station","school","pharmacy","university"};
std::string poi_entertainments[]= {"cafe", "internet_cafe", "juice_bar","restaurant", "old_restaurant", "fast_food", "food_court", "deli", "bakery", "ice_cream", "pub", "bar", "beer", "nightclub", "Theater", "theater", "cinema", "karaoke", "bicycle_rental", "boat_rental", "ice_rinks", "gambling", "casino", "stripclub",  "spa", "beauty"};

std::pair<POI_class,POI_category> getCustomedPOIClass(POIIdx poiIdx){
    // Get the BinaryDatabase instance
    auto& db = gisevo::BinaryDatabase::instance();
    
    // loop throught the arrays to check
    std::string poi_type = db.get_poi_type(poiIdx);
    POI_class poi_class = POI_class::neglegible;
    POI_category poi_category = POI_category::OTHER;
    for(int i =0; i < NUM_POI_basics; i++){
        if(poi_type == poi_basics[i]){
            poi_class = POI_class::basic;
        }
    }
    for (int i=0; i < NUM_POI_entertainment; i++){
        if(poi_type == poi_entertainments[i]){
            poi_class = POI_class::entertainment;
            if(i < Food_Bev_end){
                poi_category = POI_category::FOOD;
            }
            else if(i < Drink_end){
                poi_category = POI_category::DRINK;
            }
            else {
                poi_category = POI_category::ENTERTAINMENT;
            }
        }
    }
    for (int i=0; i < NUM_POI_subordinate; i++){
        if(poi_type == poi_subordinates[i]){
            poi_class = POI_class::subordinate;
        }
    }
    std::pair<POI_class,POI_category> poi_pair{poi_class,poi_category};
    
    return poi_pair;
}

Point2D getPOILoc(POIIdx poiIdx){
    // Get the BinaryDatabase instance
    auto& db = gisevo::BinaryDatabase::instance();
    
    LatLon poi_latlon = db.get_poi_position(poiIdx);
    
    // Get map latitude average in radians for coordinate conversion
    double map_lat_avg_rad = db.get_avg_lat_rad();
    
    // Convert to Point2D using the new coordinate conversion function
    Point2D poi_point2d = latlonTopoint(poi_latlon, map_lat_avg_rad);
    
    return poi_point2d;
}

POI_entertainment getPOIEntertainment(std::string poi_type){
    POI_entertainment type = static_cast<POI_entertainment>(0);
    for(int i =0; i<NUM_POI_entertainment; i++){
        if(poi_type == poi_entertainments[i]){
            type = static_cast<POI_entertainment>(i);
        }
    }
    return type;
}

POI_basics getPOIBasic(std::string poi_type){
    POI_basics type = static_cast<POI_basics>(0);
    for(int i =0; i<NUM_POI_basics; i++){
        if(poi_type == poi_basics[i]){
            type = static_cast<POI_basics>(i);
        }
    }
    return type;
}

POI_subordinate getPOISubordinate(std::string poi_type){
    POI_subordinate type = static_cast<POI_subordinate>(0);
    for(int i =0; i<NUM_POI_subordinate; i++){
        if(poi_type == poi_subordinates[i]){
            type = static_cast<POI_subordinate>(i);
        }
    }
    return type;
}
