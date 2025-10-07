#include "POI_setup.hpp"
#include "POI_helpers.hpp"
#include "../globals.h"
#include "../struct.h"
#include "../ms1helpers.h"
#include "../ezgl/camera.hpp"

#include <vector>
#include <string>

void sortPOI(){
    init_poi_vec();
    ezgl::point2d increment(0,3);
    for (POIIdx poiIdx = 0; poiIdx < getNumPointsOfInterest(); ++poiIdx){
        //initialize the POI_info
        auto poi_pair = getCustomedPOIClass(poiIdx);
        std::string name = getPOIName(poiIdx);
        ezgl::point2d position = getPOILoc(poiIdx);
        ezgl::point2d text_pos = position + increment;
        POI_info poi_info(position,text_pos,name,poiIdx,poi_pair.first,poi_pair.second);
        std::string poi_type_str = getPOIType(poiIdx);
        poi_type_str.erase(std::remove(poi_type_str.begin(), poi_type_str.end(), ' '),poi_type_str.end());

        switch(poi_pair.first){
            case POI_class::basic:
                POI_basics poi_basic;
                poi_basic= getPOIBasic(poi_type_str);
                poi_info.poi_customed_type = poi_basic;
                globals.poi_sorted.basic_poi[poi_basic].push_back(poi_info);
                break;
            case POI_class::entertainment:
                POI_entertainment poi_ent;
                poi_ent = getPOIEntertainment(poi_type_str);
                poi_info.poi_customed_type = poi_ent;
                globals.poi_sorted.entertainment_poi[poi_ent].push_back(poi_info);
                break;
            case POI_class::subordinate:
                POI_subordinate poi_sub;
                poi_sub =getPOISubordinate(poi_type_str);
                poi_info.poi_customed_type = poi_sub;
                globals.poi_sorted.subordinate_poi[poi_sub].push_back(poi_info);
                break;
            case POI_class::neglegible:
                poi_info.poi_customed_type = -1;
                globals.poi_sorted.neglegible_poi.push_back(poi_info);
                break;

            case POI_class::station:
                poi_info.poi_customed_type = -1;
                globals.poi_sorted.neglegible_poi.push_back(poi_info);
                break;

            default:
                poi_info.poi_customed_type = -1;
                globals.poi_sorted.neglegible_poi.push_back(poi_info);
                break;
        }
    }
}

void init_poi_vec(){
    globals.poi_sorted.entertainment_poi.resize(NUM_POI_entertainment);
    globals.poi_sorted.basic_poi.resize(NUM_POI_basics);
    globals.poi_sorted.subordinate_poi.resize(NUM_POI_subordinate);
}

// void getScalingFactor(){
//     ezgl::rectangle = get_screen
// }