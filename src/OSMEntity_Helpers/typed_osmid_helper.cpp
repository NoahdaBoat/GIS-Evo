//
// Created by montinoa on 2/28/24.
//

#include <vector>
#include <unordered_map>
#include "/cad2/ece297s/public/include/milestones/m1.h"
#include "typed_osmid_helper.hpp"
#include "OSMDatabaseAPI.h"
#include "coords_conversions.hpp"
#include <fstream>
#include <string>
#include <algorithm>


typedef unsigned int uint;
/*
 *
 */
void sort_relations() {
    std::ofstream myfile;
    myfile.open("torontoways.csv");
	std::vector<std::string> road_types_list;

	for (uint i = 0; i < getNumberOfWays(); ++i) {
		const OSMWay *current_node = getWayByIndex(i);
	
		for (uint j = 0; j < getTagCount(current_node); ++j) {
			std::pair<std::string, std::string> tag_pair = getTagPair(current_node, j);
			myfile << tag_pair.first << "," << tag_pair.second << ",\n";
		}
	}
    myfile.close();
    // use the following code if you need to write the types to a file
    //std::ofstream myfile;
    //myfile.open("allroadtypes.txt");
    //myfile << tag_pair.second << "\n";
	//myfile.close();

}

std::vector<each_relation> sort_osmrelations() {
    std::vector<each_relation> to_return;
    to_return.resize(getNumberOfRelations());
    for (uint i = 0; i < getNumberOfRelations(); ++i) {
        const OSMRelation *curr_rel = getRelationByIndex(i);
        to_return[i].members = getRelationMembers(curr_rel);
        to_return[i].relation_roles = getRelationMemberRoles(curr_rel);
        to_return[i].type = to_return[i].members[0].type();
        for (uint j = 0; j < getTagCount(curr_rel); ++j) {
            std::pair<std::string, std::string> tag_pair = getTagPair(curr_rel, j);
            if (tag_pair.first == "name") {
                to_return[i].name = tag_pair.second;
            }
        }
    }

    return to_return;

}

bool compare_relation_names(each_relation e1) {
    return e1.name == "Lake Ontario";
}

void initSubwayStations(){
    ezgl::point2d increment(0,10);
    for(unsigned node_idx =0; node_idx < getNumberOfNodes();node_idx++){
        const OSMNode *current_node = getNodeByIndex(node_idx);
            for(unsigned tag = 0; tag < getTagCount(current_node); tag++ ){
                std::pair<std::string,std::string> tag_pair = getTagPair(current_node,tag);
                if(tag_pair.first == "station" && tag_pair.second == "subway"){
                    const OSMNode* node = getNodeByIndex(node_idx);
                    ezgl::point2d position =latlonTopoint(getNodeCoords(node));
                    ezgl::point2d text_pos = position + increment;
                    std::string name;
                    for(unsigned j = 0; j< getTagCount(current_node);j++){
                        std::pair<std::string,std::string> pairs= getTagPair(current_node,j);
                        if(pairs.first=="name"){
                            name = pairs.second;
                        }
                    }
                    POIIdx idx =0;
                    POI_class rand_class = static_cast<POI_class> (0);
                    POI_info subway_info(position,text_pos,name,idx,rand_class,SUBWAY);
                    globals.poi_sorted.stations_poi.push_back(subway_info);
                    break;
                }
            }
    }
}
