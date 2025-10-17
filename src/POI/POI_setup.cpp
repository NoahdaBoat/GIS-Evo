#include "POI_setup.hpp"
#include "POI_helpers.hpp"
#include "../struct.h"
#include "../gtk4_types.hpp"
#include "../binary_loader/binary_database.hpp"

#include <vector>
#include <string>

// Local static storage for POI data (replaces globals.poi_sorted)
static POI_sorted poi_sorted;

void sortPOI(){
    init_poi_vec();
    Point2D increment{0,3};
    
    // Get the BinaryDatabase instance
    auto& db = gisevo::BinaryDatabase::instance();
    
    for (POIIdx poiIdx = 0; poiIdx < db.get_poi_count(); ++poiIdx){
        //initialize the POI_info
        auto poi_pair = getCustomedPOIClass(poiIdx);
        std::string name = db.get_poi_name(poiIdx);
        Point2D position = getPOILoc(poiIdx);
        Point2D text_pos{position.x + increment.x, position.y + increment.y};
        POI_info poi_info(position,text_pos,name,poiIdx,poi_pair.first,poi_pair.second);
        std::string poi_type_str = db.get_poi_type(poiIdx);
        poi_type_str.erase(std::remove(poi_type_str.begin(), poi_type_str.end(), ' '),poi_type_str.end());

        switch(poi_pair.first){
            case POI_class::basic:
                POI_basics poi_basic;
                poi_basic= getPOIBasic(poi_type_str);
                poi_info.poi_customed_type = poi_basic;
                poi_sorted.basic_poi[poi_basic].push_back(poi_info);
                break;
            case POI_class::entertainment:
                POI_entertainment poi_ent;
                poi_ent = getPOIEntertainment(poi_type_str);
                poi_info.poi_customed_type = poi_ent;
                poi_sorted.entertainment_poi[poi_ent].push_back(poi_info);
                break;
            case POI_class::subordinate:
                POI_subordinate poi_sub;
                poi_sub =getPOISubordinate(poi_type_str);
                poi_info.poi_customed_type = poi_sub;
                poi_sorted.subordinate_poi[poi_sub].push_back(poi_info);
                break;
            case POI_class::neglegible:
                poi_info.poi_customed_type = -1;
                poi_sorted.neglegible_poi.push_back(poi_info);
                break;

            case POI_class::station:
                poi_info.poi_customed_type = -1;
                poi_sorted.neglegible_poi.push_back(poi_info);
                break;

            default:
                poi_info.poi_customed_type = -1;
                poi_sorted.neglegible_poi.push_back(poi_info);
                break;
        }
    }
}

void init_poi_vec(){
    poi_sorted.entertainment_poi.resize(NUM_POI_entertainment);
    poi_sorted.basic_poi.resize(NUM_POI_basics);
    poi_sorted.subordinate_poi.resize(NUM_POI_subordinate);
}

// void getScalingFactor(){
//     ezgl::rectangle = get_screen
// }