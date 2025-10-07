//
// Created by montinoa on 3/22/24.
//

#pragma once

#include "StreetsDatabaseAPI.h"
#include "../globals.h"
#include "ezgl/application.hpp"
#include <queue>
#include <vector>

extern ezgl::application* global_access;

extern int timesum;

struct Wave_Elm {
    int node_id; // id of the node
    int edge_id; // id of the edge used to reach this node
    int street_index; // the index of the road that this street segment belongs to
    double travel_time; // time taken to get here
    double time_to_end; // time to get to the next node, factoring in distance and maximum speed
    double estimated_time; // estimated time taken to get to the end, factoring in the current time taken
    //Wave_Elm* prev_elm;
    double distance_to_end;

    // constructor for A*
    Wave_Elm(int id, int edge, int street, double time, double to_end, double est_time, double dist_to_end)
            : node_id(id), edge_id(edge), street_index(street), travel_time(time), time_to_end(to_end),
              estimated_time(est_time), distance_to_end(dist_to_end) {}

    Wave_Elm(int id, int edge, int street, double time)
            : node_id(id), edge_id(edge), street_index(street), travel_time(time) {}

};

struct comparator {
    bool operator()(Wave_Elm const& a, Wave_Elm const& b) const {
        return a.estimated_time > b.estimated_time;
    }
};

struct comparator_dijkstra {
    bool operator()(Wave_Elm const& a, Wave_Elm const& b) const {
        return a.travel_time > b.travel_time;
    }
};

struct Search_Node {
    //std::vector<int> outgoing_edges; // all edges coming from this node
    int edge_id; // edge used to reach this node
    int node_id; // intersection before this node
    double best_time; // the best time to this node
    bool visited; // have we visited this intersection before?
    // constructor
    Search_Node() : best_time(std::numeric_limits<double>::max()), visited(false) {}
};

std::vector<StreetSegmentIdx> aStarAlgorithm(int start_id, int end_id, int turn_penalty);


extern StreetSegmentIdx street_to_highlight;