#ifndef POI_setup
#define POI_setup

#include "/cad2/ece297s/public/include/streetsdatabase/StreetsDatabaseAPI.h"
#include "/cad2/ece297s/public/include/streetsdatabase/OSMDatabaseAPI.h"



#include <string>


enum POI_class{
    basic=0,
    entertainment,
    subordinate,
    neglegible,
    station
};

enum POI_basics {
    hospital=0,
    emergency_room,
    bus_station,
    school,
    pharmacy,
    university
};
enum POI_entertainment{
    cafe=0, 
    internet_cafe, 
    juice_bar,
    restaurant,
    old_restaurant, 
    fast_food, 
    food_court, 
    deli,
    bakery, 
    ice_cream, 
    pub, 
    bar, 
    beer, 
    nightclub, 
    Theater, 
    theater, 
    cinema, 
    karaoke, 
    bicycle_rental, 
    boat_rental, 
    ice_rinks, 
    gambling, 
    casino, 
    stripclub, 
    spa, 
    beauty
};

enum POI_subordinate{
    clinic=0,
    police,
    library,
    food_bank,
    shelter,
    research_institute,
    parking,
    atm,
    toilets
};

enum POI_category{
    BASIC =0,
    FOOD,
    DRINK,
    HOSPITAL,
    SHOPPING,
    NEGLECT,
    SUBORDINATE,
    ENTERTAINMENT,
    SUBWAY,
    OTHER,
    HIGHLIGHT,
    GROCERY,
    PIN
};

// initialize the global variable that contains all the pois sorted
void init_poi_vec();

// sorts all of the pois into classes, categories, and type
void sortPOI();
//void POITypes(std::unordered_map<std::string, POIIdx>&);

#endif