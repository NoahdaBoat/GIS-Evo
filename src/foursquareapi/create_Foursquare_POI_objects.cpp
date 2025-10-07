#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <iomanip>
#include <string>
#include <vector>
#include <math.h>

#include "foursquarefunctions.hpp"
#include "../../resources/jsoncpp-master/include/json/json.h"
#include "globals.h"
#include "coords_conversions.hpp"
#include "POI_helpers.hpp"

class Poi
{
public:
    std::string getCity()
    {
        return this->city;
    }
    std::string getCountry()
    {
        return this->country;
    }
    std::string getCategory()
    {
        return this->category;
    }
    std::string getName()
    {
        return this->name;
    }
    std::string getAddress()
    {
        return this->address;
    }
    std::string getwebsite()
    {
        return this->website;
    }
    double getLat()
    {
        return this->lat;
    }
    double getLon()
    {
        return this->lon;
    }
    double getRating()
    {
        return this->rating;
    }

    Poi(std::string pcity, std::string pcountry, std::string pname, std::string pcategory, std::string paddress, std::string pwebsite, double plat, double plon, double prating)
    {
        this->city = pcity;
        this->country = pcountry;
        this->name = pname;
        this->category = pcategory;
        this->address = paddress;
        this->website = pwebsite;
        this->lat = plat;
        this->lon = plon;
        this->rating = prating;
    }

private:
    std::string city;
    std::string country;
    std::string name;
    std::string category;
    std::string address;
    std::string website;
    double lat;
    double lon;
    double rating;
};

int parse_foursquare_data(std::string category, std::string city, std::string country)
{
    std::vector<Poi> pois;

//    if (argc != 4)
//    {
//        fprintf(stderr, "Usage: %s Category City CountryCode\n", argv[0]);
//        exit(EXIT_FAILURE);
//    }

    std::string paramCategory = category;
    std::string paramCity = city;
    std::string paramCountryCode = country;

    Json::Value jsonData;
    JSONCPP_STRING jsonError;
    Json::CharReaderBuilder builder;

    std::ifstream poi_file(paramCategory + paramCity + paramCountryCode + ".json");
    // To add - check for file open errors
    poi_file >> jsonData;

    //std::cout << "Successfully parsed JSON data" << std::endl;

    std::string addressString;
    std::string nameString;
    std::string websiteString;
    double rating;
    double lat;
    double lon;

    // not sure if this is required - not likely 
//    std::cout << "Search latitude: " << jsonData["context"]["geo_bounds"]["circle"]["center"]["latitude"] << "\n";
//    std::cout << "Search longitude: " << jsonData["context"]["geo_bounds"]["circle"]["center"]["longitude"] << "\n";
//    std::cout << std::endl;

    for (Json::Value::ArrayIndex i = 0; i != jsonData["results"].size(); i++) {
        nameString = jsonData["results"][i]["name"].asString();
        addressString = jsonData["results"][i]["location"]["address"].asString();
        if (addressString.empty())
        {
            addressString = jsonData["results"][i]["location"]["formatted_address"].asString();
        }
        websiteString = jsonData["results"][i]["website"].asString();
        lat = jsonData["results"][i]["geocodes"]["main"]["latitude"].asDouble();
        lon = jsonData["results"][i]["geocodes"]["main"]["longitude"].asDouble();
        rating = jsonData["results"][i]["rating"].asDouble();
        rating = std::round(rating * 10.0) / 10.0;
        Poi poi(paramCity, paramCountryCode, nameString, paramCategory, addressString, websiteString, lat, lon, rating);
        pois.push_back(poi);
    }

    // optional print
//    for (unsigned int i = 0; i < pois.size(); i++) {
//        std::cout << "City: " << pois[i].getCity() << std::endl;
//        std::cout << "Category: " << pois[i].getCategory() << std::endl;
//        std::cout << "Name: " << pois[i].getName() << std::endl;
//        std::cout << "Address: " << pois[i].getAddress() << std::endl;
//        std::cout << "Website: " << pois[i].getwebsite() << std::endl;
//        std::cout << "Latitude: " << pois[i].getLat() << std::endl;
//        std::cout << "Longitude: " << pois[i].getLon() << std::endl;
//        std::cout << "Rating: " << pois[i].getRating() << std::endl;
//        std::cout << std::endl;
//    }
    if (category == "restaurants") {
        globals.city_restaurants.resize(pois.size());
        for (uint i = 0; i < pois.size(); ++i) {
            globals.city_restaurants[i].poi_name = pois[i].getName();
            globals.city_restaurants[i].address = pois[i].getAddress();
            globals.city_restaurants[i].city = pois[i].getCity();
            globals.city_restaurants[i].rating = pois[i].getRating();
            double x_pos = lon_to_x(pois[i].getLon());
            double y_pos = lat_to_y(pois[i].getLat());
            globals.city_restaurants[i].poi_loc = {x_pos, y_pos};
            globals.city_restaurants[i].website = pois[i].getwebsite();
            globals.city_restaurants[i].inner_category = pois[i].getCategory();
            globals.city_restaurants[i].country = pois[i].getCountry();
            globals.city_restaurants[i].top_category = getPOIEntertainment(pois[i].getCategory());
            globals.city_restaurants[i].poi_class = POI_class::entertainment;
            globals.city_restaurants[i].poi_category = POI_category::FOOD;
            LatLon temp = LatLon(static_cast<float>(pois[i].getLat()), static_cast<float>(pois[i].getLon()));
            globals.city_restaurants[i].pos = temp;
        }
    }
    else if (category == "shops") {
        globals.city_shops.resize(pois.size());
        for (uint i = 0; i < pois.size(); ++i) {
            globals.city_shops[i].poi_name = pois[i].getName();
            globals.city_shops[i].address = pois[i].getAddress();
            globals.city_shops[i].city = pois[i].getCity();
            globals.city_shops[i].rating = pois[i].getRating();
            double x_pos = lon_to_x(pois[i].getLon());
            double y_pos = lat_to_y(pois[i].getLat());
            globals.city_shops[i].poi_loc = {x_pos, y_pos};
            globals.city_shops[i].website = pois[i].getwebsite();
            globals.city_shops[i].inner_category = pois[i].getCategory();
            globals.city_shops[i].country = pois[i].getCountry();
            globals.city_shops[i].top_category = getPOIEntertainment(pois[i].getCategory());
            globals.city_shops[i].poi_class = POI_class::entertainment;
            globals.city_shops[i].poi_category = POI_category::FOOD;
            LatLon temp = LatLon(static_cast<float>(pois[i].getLat()), static_cast<float>(pois[i].getLon()));
            globals.city_shops[i].pos = temp;
        }
    }
    return 0;
}