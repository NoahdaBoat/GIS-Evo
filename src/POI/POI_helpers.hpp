#pragma once

#include "POI_setup.hpp"
#include "../struct.h"
#include "../ezgl/point.hpp"



#include <string>
#define NUM_POI_class 5
#define NUM_POI_basics 6
#define NUM_POI_entertainment 26
#define NUM_POI_subordinate 11
#define Food_Bev_end 10
#define Drink_end 13
#define Theater_end 17
#define Sport_end 20
#define Ent_end 23
#define Beauty_end 26

// returns a pair, first element is larger poi class, and second is the small poi category
std::pair<POI_class,POI_category> getCustomedPOIClass(POIIdx poiIdx);

//returns the position of POI in x,y coord
ezgl::point2d getPOILoc(POIIdx poiIdx);

//convert string to POI_basic
POI_basics getPOIBasic(std::string poi_type);

//convert string to POI_entertainment
POI_entertainment getPOIEntertainment(std::string poi_type);

//convert string to POI_subordinate
POI_subordinate getPOISubordinate(std::string poi_type);

