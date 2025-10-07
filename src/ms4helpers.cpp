//
// Created by montinoa on 4/5/24.
//

#include "m4.h"
#include "m3.h"
#include "ms4helpers.hpp"
#include "globals.h"
#include "astaralgo.hpp"
#include "sort_streetseg/streetsegment_info.hpp"
#include <omp.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <stdlib.h>
#include <random>
#include <limits>

// This function calls multi-dijkstra to find the best travel routes between all dropoff points, and depots to dropoff points
// We don't need routes between depots
void compute_all_travel_times(const std::vector<IntersectionIdx>& of_interest,
                              const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
                              std::vector<std::vector<OneRoute>>& route_matrix,
                              const float& turn_penalty) {

    // loop through all deliveries, tack on the depots to the nodes to search

    // need this for parallel operation without race conditions
    const std::vector<street_segment_info> local_all_segments = globals.all_street_segments;

//    auto start = std::chrono::high_resolution_clock::now();

    #pragma omp parallel for
    for (auto& i : of_interest) {
        multi_dijkstra(i, of_interest, turn_penalty, route_matrix, intersection_to_index, local_all_segments);
    }

//    auto end = std::chrono::high_resolution_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
//    std::cout << "Took " << duration.count() << std::endl;
//    std::cout << "Done" << std::endl;
}

void multi_dijkstra(const IntersectionIdx start,
                    const std::vector<IntersectionIdx> of_interest,
                    const float turn_penalty,
                    std::vector<std::vector<OneRoute>>& route_matrix,
                    const std::unordered_map<IntersectionIdx, int> intersection_to_index,
                    const std::vector<street_segment_info> local_all_segments) {

    // vector for our path of nodes
    std::vector<StreetSegmentIdx> route_elements;

    int first_array_index = 0;
    auto first_index = intersection_to_index.find(start);
    if (first_index != intersection_to_index.end()) {
        first_array_index = first_index->second;
    }

    // holds a struct of nodes we have searched before
    std::vector<Search_Node> visited;
    visited.resize(getNumIntersections());

    // have we found all routes we were looking for?
    bool found_all = false;

    // this will keep track if we have found all intersections
    int found_routes = 0; // will be equal to of_interest.size() when we have found all routes

    // set up the first element, the start intersection
    // don't know which edge or street we used, also no time so far
    Wave_Elm first_elm(start, 0, 0, 0);

    // already searched the beginning intersection
    Search_Node first_node;
    first_node.edge_id = 0; // will be incorrect for the first node
    first_node.best_time = 0;
    first_node.node_id = -1;

    visited[start] = first_node;

    // queue of nodes to search
    std::priority_queue<Wave_Elm, std::vector<Wave_Elm>, comparator_dijkstra> wave_front;

    wave_front.push(first_elm);

    // loop until the queue is empty or the node is found
    while (!wave_front.empty() && !found_all) {

        Wave_Elm current_elm = wave_front.top();

        if (!wave_front.empty()) {
            wave_front.pop();
        }

        int current_elm_id = current_elm.node_id;

        if (visited[current_elm_id].visited) {
            continue;
        }

        visited[current_elm_id].visited = true;

        // check if the current intersection exists inside the vector of intersections of interest
        if (std::find(of_interest.begin(), of_interest.end(), current_elm_id) != of_interest.end()) {
            found_routes++;
            int current_inter = current_elm_id;
            int next_inter;

            while (visited[current_inter].node_id != -1) {
                route_elements.push_back(visited[current_inter].edge_id);
                next_inter = visited[current_inter].node_id;
                current_inter = next_inter;
            }
            std::reverse(route_elements.begin(), route_elements.end());

            // get the corresponding array index from the unordered map
            int array_index = 0;
            auto index = intersection_to_index.find(current_elm_id);
            if (index != intersection_to_index.end()) {
                array_index = index->second;
            }

            route_matrix[first_array_index][array_index].route = route_elements;
            route_matrix[first_array_index][array_index].start = start;
            route_matrix[first_array_index][array_index].end = current_elm_id;
            route_matrix[first_array_index][array_index].travel_time = computePathTravelTime(turn_penalty, route_elements);

            route_elements.clear();

            if (found_routes == of_interest.size()) {
                found_all = true;
            }

        }

        if (!found_all) {

            std::vector<StreetSegmentIdx> outgoing_streets = findStreetSegmentsOfIntersection(current_elm_id);

            // loop through all the outgoing streets of the current intersection (node)

            for (auto i: outgoing_streets) {
                bool invalid_check = false;

                IntersectionIdx next_intersection;

                // if the current node is at from, then the next node is at to
                // if the current node is at to, and it's not a one way street, then the next node is at from

                if (current_elm_id == local_all_segments[i].from) {
                    next_intersection = local_all_segments[i].to;
                } else if (!local_all_segments[i].oneWay) {
                    next_intersection = local_all_segments[i].from;
                } else {
                    invalid_check = true;
                }

                // if this node was popped from the wavefront before, no sense in checking it
                // if the road is one way in the wrong direction, skip it
                if (invalid_check || visited[next_intersection].visited) {
                    continue;
                }

                double distance_to_next = findStreetSegmentLength(i);

                Search_Node next_node;
                next_node.edge_id = i;
                next_node.node_id = current_elm_id;

                // determine the best time to reach this node so far
                next_node.best_time =
                        current_elm.travel_time + distance_to_next / local_all_segments[i].speedLimit;

                // account for the turn penalty if we change streets
                if (local_all_segments[i].street != current_elm.street_index) {
                    next_node.best_time += turn_penalty;
                }

                // only add this new node to the wavefront if we found a shorter route to it
                if (next_node.best_time < visited[next_intersection].best_time) {
                    visited[next_intersection] = next_node;
                    // get the distance to the destination from where we are now

                    double travel_time = next_node.best_time;

                    Wave_Elm next_elm(next_intersection, i, local_all_segments[i].street, travel_time);

                    wave_front.push(next_elm);

                }
            }
        }
    }
}


std::vector<IntersectionIdx> find_unique_intersections(const std::vector<DeliveryInf> &deliveries, const std::vector<IntersectionIdx>& depots) {
    std::vector<IntersectionIdx > unique_intersections;
 //   int new_size = (2*deliveries.size())+(depots.size());
  //  unique_intersections.resize(new_size);
//    for(int i = 0, j= 0; i < deliveries.size(); i++,j+=2) {
//        unique_intersections[j] = deliveries[i].dropOff;
//        unique_intersections[j + 1] = deliveries[i].pickUp;
//    }
    for (auto& i : deliveries) {
        unique_intersections.push_back(i.dropOff);
        unique_intersections.push_back(i.pickUp);
    }

    for (auto& i : depots) {
        unique_intersections.push_back(i);
    }

    // put duplicated element next to one another
    std::sort(unique_intersections.begin(),unique_intersections.end());
    //all elements after iterator will be the duplicated ones
    auto iterator = std::unique(unique_intersections.begin(),unique_intersections.end());
    //remove duplicated elements in the vector
    unique_intersections.erase(iterator,unique_intersections.end());

    return unique_intersections;
}

void preloadDeliveryStops(const std::vector<DeliveryInf> &deliveries){
    int id =0;
    for(auto& info : deliveries){
        Delivery_Stop pick_up_stop(Stop_Type::PICKUP,id,info.pickUp);
        Delivery_Stop drop_off_stop(Stop_Type::DROPOFF,id,info.dropOff);
    }
}

void loadDeliveryDetails(const std::vector<DeliveryInf> &deliveries,const std::vector<IntersectionIdx>&depots, std::vector<IntersectionIdx>& pick_ups, std::vector<IntersectionIdx>& drop_offs, std::vector<IntersectionIdx>& unique_intersections){
   for(auto& info : deliveries){
       //populate unique intersections
       unique_intersections.push_back(info.dropOff);
       unique_intersections.push_back(info.pickUp);

       //populate delivery info
       //check pick up
       if(globals.delivery_info.count(info.pickUp) == 0) {
           // if it is the first time seeing the intersection
           Delivery_details details(true, false,false);
           globals.delivery_info[info.pickUp] = details;
           globals.delivery_info[info.pickUp].corres_dropoff.push_back(info.dropOff);
           //push the intersection into pick_ups
           pick_ups.push_back(info.pickUp);
       }
       else {
           // already seen this intersection before
           if(globals.delivery_info[info.pickUp].drop_off) {
               // has seen this as a drop_off
               globals.delivery_info[info.pickUp].pick_up = true;
               pick_ups.push_back(info.pickUp);
           }
           globals.delivery_info[info.pickUp].corres_dropoff.push_back(info.dropOff);
       }
       //check drop off
       if(globals.delivery_info.count(info.dropOff)==0){
           // if it is the first time seeing the intersection
           Delivery_details details(false,true,false);
           globals.delivery_info[info.dropOff] = details;
           globals.delivery_info[info.dropOff].corres_pickup.push_back(info.pickUp);
       }
       else{
           // already seen this intersection before
           if(globals.delivery_info[info.pickUp].pick_up) {
               //has seen this as a pick_up before
               globals.delivery_info[info.dropOff].drop_off = true;
               drop_offs.push_back(info.dropOff);
           }
           globals.delivery_info[info.dropOff].corres_pickup.push_back(info.pickUp);
       }
   }
    for (auto& i : depots) {
        unique_intersections.push_back(i);
    }

    // put duplicated element next to one another
    std::sort(unique_intersections.begin(),unique_intersections.end());
    //all elements after iterator will be the duplicated ones
    auto iterator = std::unique(unique_intersections.begin(),unique_intersections.end());
    //remove duplicated elements in the vector
    unique_intersections.erase(iterator,unique_intersections.end());

}

bool checkLegalNode(std::vector<IntersectionIdx> &path) {
    int i = 0;
    for(int j = 1; j<path.size()-2; j++) {
        IntersectionIdx node = path[j];
        Delivery_details* current = &globals.delivery_info[node];
        if(!current->drop_off) {
            //if it is only a pick up node, don't need to check for legality
            globals.delivery_info[node].visited = true;
            continue;
        }
        else {
            // need to check if the corresponding pick up node is visited
            current->visited = true;
            for(int k = 0; k < current->corres_pickup.size(); k++) {
                // loop through all the corresponding pick up nodes
                Delivery_details* pickup = &globals.delivery_info[current->corres_pickup[k]];
                if(!pickup->visited) {
                    clearVisitedNodes(path, i);
                    return false;
                }
            }
        }
        i++;
    }
    clearVisitedNodes(path, i);
    return true;
}

bool checkLegalNodeParallel(const std::vector<IntersectionIdx> path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info) {

    for(int i = 1; i < path.size()-2; i++) {
        IntersectionIdx node = path[i];
        Delivery_details* current = &delivery_info[node];
        if(!current->drop_off) {
            //if it is only a pick up node, don't need to check for legality
            delivery_info[node].visited = true;
            continue;
        }
        else {
            // need to check if the corresponding pick up node is visited
            current->visited = true;
            for(int j = 0; j < current->corres_pickup.size(); j++) {
                // loop through all the corresponding pick up nodes
                Delivery_details* pickup = &delivery_info[current->corres_pickup[j]];
                if (!pickup->visited) {
                    return false;
                }
            }
        }
    }
    return true;
}

void clearVisitedNodes(std::vector<IntersectionIdx> &path, int index){
    for(int i = 0; i <= index; i++){
        Delivery_details* current = &globals.delivery_info[path[i]];
        current->visited = false;
    }
}

void preloadKeys(const std::vector<IntersectionIdx>& key_intersections, std::unordered_map<IntersectionIdx, int>& returning_map){
    for (int i = 0; i < key_intersections.size(); i++) {
        returning_map.insert({key_intersections[i], i});
    }
}

void updateAvailableStops(IntersectionIdx new_stop, std::vector<IntersectionIdx> &available_stops, std::unordered_map<IntersectionIdx ,Delivery_details>& infos){
        Delivery_details* current = &infos[new_stop];
        if(!current->pick_up){
            //if it is only a drop-off node, no new available nodes, mark as visited
            current->visited = true;
           return;
        }
        else{
            // need to check if there are new available nodes
            current->visited = true;
            //maybe parallerize this for loop
            for(int i = 0; i< current->corres_dropoff.size(); i++){
                // loop through all the corresponding drop off nodes
                Delivery_details* drops = &infos[current->corres_dropoff[i]];
                if(drops->added){
                    // skip this drop off if it has already been added to the available stops
                    continue;
                }
               // check if all the pickup stops is visited for this drop off
               bool if_legal = true;
               for(const auto& check_pick:drops->corres_pickup){
                   if(!infos[check_pick].visited){
                       // not all pick up nodes are visisted, do not add
                       if_legal = false;
                       break;
                   }
               }
               // insert if all pick up are visisted and has not been added
               if(if_legal){
                   available_stops.push_back(current->corres_dropoff[i]);
                   drops->added = true;
               }
            }

        }
}

std::vector<CourierSubPath> indexToSubPath(const std::vector<IntersectionIdx>& path, const std::vector<std::vector<OneRoute>>& routes_matrix, std::unordered_map<IntersectionIdx, int>& intersection_to_index){
    std::vector<CourierSubPath> sub_path;
    for(int i = 0; i < path.size()-1; i++) {
        CourierSubPath current_path;
        const OneRoute* route = &routes_matrix[intersection_to_index[path[i]]][intersection_to_index[path[i+1]]];
        current_path.intersections = std::make_pair(route->start,route->end);
        current_path.subpath = route->route;
        sub_path.push_back(current_path);
    }
    return sub_path;
}

[[maybe_unused]] std::vector<IntersectionIdx> twoOptImplementation (
                                                   const std::vector<IntersectionIdx>& start_path,
                                                   const double start_path_cost,
                                                   const std::vector<std::vector<OneRoute>>& routes_matrix,
                                                   const long time_taken,
                                                   const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
                                                   const std::unordered_map<IntersectionIdx, Delivery_details> delivery_info,
                                                   const int max_bound,
                                                   const int min_bound) {

    std::vector<IntersectionIdx> path = start_path;
    std::vector<IntersectionIdx> new_path = path;
    double cost = start_path_cost;
    double new_cost = start_path_cost;
    bool skip_iter = false;
    bool timeout = false;
    //static thread_local std::random_device rd;
    //static thread_local std::mt19937 rng(rd());
    static thread_local std::mt19937 gen_rand(std::random_device{}());
    std::uniform_int_distribution<int> get_rand(min_bound,max_bound);


    auto start_time = std::chrono::high_resolution_clock::now();

    while (!timeout) {
        //std::cout << "HereS" << std::endl;
        for (int i = min_bound; i < max_bound; ++i) {
            //std::cout << i << std::endl;
            if (skip_iter || timeout) {
                skip_iter = false;
                //std::cout << "HereS" << std::endl;
                break;
            }
       // int i = get_rand(gen_rand);
        int max_end = std::min((int)(i + path.size()/7), static_cast<int>(path.size()) - 1);
        int min_end = std::max(1, (int)(i - path.size()/7));
        for (int j = min_end; j <max_end;  ++j){
         //   for (int j = 1; j < path.size()-1; ++j) {
                //std::uniform_int_distribution<int> rud(1, path.size() - max_bound - 1);
                //int i = rud(rng);
                //std::uniform_int_distribution<int> rud2(1, max_bound);
                //int j = rud2(rng);

                new_path = twoOptVTwo(path, i, j, delivery_info);

                if (new_path != path) {
                    new_cost = pathCost(new_path, routes_matrix, intersection_to_index);
                }

                auto current_time = std::chrono::high_resolution_clock::now();
                auto duration_cost = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
                if (duration_cost.count() + time_taken >= 48) {
                    //std::cout << "HereT" << std::endl;
                    timeout = true;
                    break;
                }

                if (new_cost < cost) {
                    path = new_path;
                    cost = new_cost;
                    //std::cout << new_cost << std::endl;
                    skip_iter = true;
                    break;
                }

            }
        }
        //auto end = std::chrono::high_resolution_clock::now();
        //auto duration = std::chrono::duration_cast<std::chrono::seconds>(end-start_time);
        //run_time = duration.count();
        //std::cout << run_time << std::endl;
    }
    return path;
}

std::vector<IntersectionIdx> annealingTwoOpt  (int temperature,
                                               const double alpha,
                                               struct drand48_data buffer,
                                               const std::vector<IntersectionIdx>& start_path,
                                               const double start_path_cost,
                                               const std::vector<std::vector<OneRoute>>& routes_matrix,
                                               const long time_taken,
                                               const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
                                               const std::unordered_map<IntersectionIdx, Delivery_details> delivery_info,
                                               const int max_bound,
                                               const int min_bound) {

    std::vector<IntersectionIdx> path = start_path;
    std::vector<IntersectionIdx> new_path = path ;
    std::vector<IntersectionIdx> global_path = path;
    double cost = start_path_cost;
    double new_cost = start_path_cost;
    double global_cost = start_path_cost;
    bool timeout = false;
    double delta_c = 0;
    double random = 0;
    //static thread_local std::random_device rd;
    //static thread_local std::mt19937 rng(rd());
    static thread_local std::mt19937 gen_rand(std::random_device{}());
    std::uniform_int_distribution<int> get_rand_i(min_bound,max_bound);



    auto start_time = std::chrono::high_resolution_clock::now();

    while (!timeout) {
        int i =get_rand_i(gen_rand);

        int max_end = std::min((int)(i)+20, static_cast<int>(path.size()) - 1);
        int min_end = std::max(1, (int)(i)-5);
        std::uniform_int_distribution<int> get_rand_j(min_end,max_end);
        if(max_bound-min_bound > 10 && max_bound - min_bound < 15){
            max_end = std::min((int)(i + (max_bound-min_bound))+4, static_cast<int>(path.size()) - 1);
            min_end = std::max(1,((int)(i - (max_bound-min_bound)-1) ));
        }
        else if(max_bound-min_bound >= 15){
            max_end = std::min((int)(i + 10)+4, static_cast<int>(path.size()) - 1);
            min_end = std::max(1,((int)(i - 5)-1));
        }

        for (int j = min_end; j <max_end;  ++j) {
            int temp = j;
            if(max_end - min_end > 7){
                j = get_rand_j(gen_rand);
            }

            std::random_device rand_dev;
            thread_local std::mt19937 generator(rand_dev());
            std::uniform_real_distribution<double> get_rand(0,1);
            static thread_local std::mt19937 rng(std::random_device{}());

            std::uniform_int_distribution<int> to2(0,4);
            int* numbers;
            int long_path[]={0, 2, 2, 2,2};
            int short_path[]= {0,0,2,2,2};

            if(path.size()>130) {
                numbers = long_path;
            }
            else{
                numbers = short_path;
            }

            int select;
            // equal chance of getting a decimal between 0 to 1
            double probability = get_rand(generator);
            //50% of running 2-opt
            double compare = temperature > 5?0.75:0.15;
            if(probability < compare){
                select = 1;
            }
            else{
                select = (*(numbers+to2(generator)));
            }

            // select which perturbation to apply
            switch(select){
                case 0:
                    new_path = perturbationSwap(path, delivery_info);
                    break;

                case 1:
                    new_path = twoOptVTwo(path, i, j, delivery_info);
                    break;

                case 2:
                    new_path =  perturbationMoveOne(path, delivery_info);
                    break;

                case 3:
                    new_path = swapAndShift(path,delivery_info);
                    break;
                default:
                    break;
            }


            if (new_path != path) {
                new_cost = pathCost(new_path, routes_matrix, intersection_to_index);
            }

            auto current_time = std::chrono::high_resolution_clock::now();
            auto duration_cost = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
            if (duration_cost.count() + time_taken >= 48) {
                timeout = true;
                break;
            }

            delta_c = new_cost - cost;
            drand48_r(&buffer, &random);
            if (delta_c < 0 || random < exp(-delta_c / temperature)) {
                if(new_cost <global_cost){
                    global_cost = new_cost;
                    global_path = new_path;
                }
                path = new_path;
                cost = new_cost;
                break;
            }
            j = temp+1;
        }
        if (temperature < 0.00001) {
            temperature = 0.00001;
        }
        else {
            temperature *= alpha;
        }
    }
    if (global_cost <new_cost) {
        path = global_path;
    }
    return path;
}



std::vector<IntersectionIdx> simulatedAnnealing(int temperature,
                        const std::vector<IntersectionIdx> start_path,
                        const double start_path_cost,
                        const int num_perturbations,
                        const std::vector<std::vector<OneRoute>> routes_matrix,
                        struct drand48_data buffer,
                        const double alpha,
                        const double time_taken,
                        const std::unordered_map<IntersectionIdx, int> intersection_to_index) {

    auto start = std::chrono::high_resolution_clock::now();
    double execution_time = 0;
    std::vector<IntersectionIdx> path = start_path;
    std::vector<IntersectionIdx> new_path = start_path;
    double cost = start_path_cost;
    double new_cost = 0;
    double delta_c = 0;
    double random = 0;
    // start point for 2 opt
    //(lrand48_r(&buffer, &random2) % 49) + 1;
    // keep making perturbations while the time is less than 45 seconds
    while (time_taken + execution_time < 45) {


        for (int i = 0; i < num_perturbations; ++i) {
            new_path = perturbationTwoOpt(path);
            //if (checkLegalNode(path)) {
                new_cost = pathCost(new_path, routes_matrix, intersection_to_index);
                delta_c = new_cost - cost;
                drand48_r(&buffer, &random);
                if (delta_c < 0 || random < exp(-delta_c / temperature)) {
                    path = new_path;
                    cost = new_cost;
                }
            //}
        }
        if (temperature < 0.00001) {
            temperature = 0.00001;
        }
        else {
            temperature *= alpha;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end-start);
        execution_time = duration.count();
       // std::cout << execution_time << std::endl;
    }
    return path;
}

double pathCost(const std::vector<IntersectionIdx>& path,
                const std::vector<std::vector<OneRoute>>& routes_matrix,
                const std::unordered_map<IntersectionIdx, int>& intersection_to_index) {

    double cost = 0;

    for (uint i = 0; i < path.size()-1; ++i) {
        auto find_first = intersection_to_index.find(path[i]);
        auto find_second = intersection_to_index.find(path[i+1]);
        cost += routes_matrix[find_first->second][find_second->second].travel_time;
    }

    return cost;
}

std::vector<IntersectionIdx> perturbationSwap(std::vector<IntersectionIdx>& path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info){

   thread_local static std::mt19937 rng(std::random_device{}());
    std::vector<IntersectionIdx> path2 = path;
    // not enough elements for meaningful swap
    if (path2.size() <= 3){
        return path2;
    }

    // min = 1, max = path.size() - 2
    std::uniform_int_distribution<int> random_index(1, path2.size() - 2);
    int swap_1 = random_index(rng);
    int swap_2;

    do {
        swap_2 = random_index(rng);
    } while (swap_2 == swap_1);

    std::swap(path2[swap_1], path2[swap_2]);

    if (checkLegalNodeParallel(path2, delivery_info)) {
        return path2;
    }
    else {
        return path;
    }
}

std::vector<IntersectionIdx> perturbationTwoOpt(std::vector<IntersectionIdx> path) {
    // not enough elements for meaningful 2opt
    if (path.size() <= 3){
        return path;
    }

    // min = 1, max = path.size() - 3
    int start = generateDistribution(1, path.size()-4);
    // min = start + 1, max = path.size() - 2
    int end = start + 3;

    std::reverse(path.begin() + start + 1, path.begin() + end);
    return path;
}

std::vector<IntersectionIdx> twoOptVTwo(std::vector<IntersectionIdx>& path, const int index_1, const int index_2 ,std::unordered_map<IntersectionIdx, Delivery_details> delivery_info) {
    std::vector<IntersectionIdx> path2 = path;
    if (path.size() <= 5) {
        return path;
    }

    std::reverse((path2.begin() + index_1), (path2.begin() + index_2));

    if (checkLegalNodeParallel(path2, delivery_info)) {
      //  std::cout<<"legal 2-opt at start:"<<index_1<<" end: "<<index_2<<std::endl;
        return path2;
    }
    else {
      //  std::cout<<"illegal 2-opt at start:"<<index_1<<" end: "<<index_2<<std::endl;
        return path;
    }
}


std::vector<IntersectionIdx> perturbationMoveOne(std::vector<IntersectionIdx>& path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info){

    thread_local static std::mt19937 rng(std::random_device{}());
    std::vector<IntersectionIdx> path2 = path;

    // not enough elements for meaninful move
    if (path2.size() <= 3){
        return path;
    }

    // min = 1, max = path.size() - 2
    std::uniform_int_distribution<int> random_index(1, path2.size() - 2);
    int start = random_index(rng);
    int end;

    do {
        end = random_index(rng);
    } while (end == start);

    if (start < end){
        std::rotate(path2.begin() + start, path2.begin() + start + 1, path2.begin() + end + 1);
    }
    else{
        std::rotate(path2.begin() + end, path2.begin() + end + 1, path2.begin() + start + 1);
    }

    if (checkLegalNodeParallel(path2, delivery_info)) {
        return path2;
    }
    else {
        return path;
    }
}

// std::vector<IntersectionIdx> perturbeTravelRoute(std::vector<IntersectionIdx>& path){
//     int num_perturbation = 3;
//     std::random_device rand_dev;
//     std::mt19937 generator(rand_dev());
//     std::uniform_real_distribution<double> get_rand(0,1);
//     thread_local std::mt19937 rng(std::random_device{}());
//     std::uniform_int_distribution<int> to2(0,2);
//     int select;
//     // equal chance of getting a decimal between 0 to 1
//     double probability = get_rand(generator);
//     //85% of running 2-opt
//     if(probability < 0.85){
//         select = 1;
//     }
//     else{
//         select = to2(rng)*2-2;
//     }

//     switch(select){
//         case 0:

//             return perturbationSwap(path);

//         case 1:
//             //return perturbationTwoOpt(path);

//         case 2:
//             return perturbationMoveOne(path);

//         default:
//             return path;
//     }
// }

// https://stackoverflow.com/questions/21237905/how-do-i-generate-thread-safe-uniform-random-numbers
int generateDistribution(const int& min, const int& max) {
    static thread_local std::mt19937 generator;
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(generator);
}


std::vector<IntersectionIdx> greedyAlgo (std::vector<IntersectionIdx>& pick_ups,std::vector<std::vector<OneRoute>>& routes_matrix, IntersectionIdx depot, const std::unordered_map<IntersectionIdx, int>& intersection_to_index) {
        std::unordered_map<IntersectionIdx ,Delivery_details> infos = globals.delivery_info;
        IntersectionIdx prev_node = depot;
        std::vector<IntersectionIdx> available_stops = pick_ups;
        std::vector<IntersectionIdx> path;
        path.push_back(depot);
        int closest_index = 0;
        IntersectionIdx closest_node;
        while (!available_stops.empty()) {
            //initialize useful parameters
            double fastest_time = DBL_MAX;

            //loop through all the legal next stop
            for (int i = 0; i < available_stops.size(); i++) {
                IntersectionIdx node = available_stops[i];
//                double current_time = routes_matrix[intersection_to_index[prev_node]][intersection_to_index[node]].travel_time;
                double current_time = routes_matrix.at(intersection_to_index.at(prev_node)).at(intersection_to_index.at(node)).travel_time;
                if (current_time < fastest_time) {
                    fastest_time = current_time;
                    closest_node = node;
                    closest_index = i;
                }
            }
            //insert the closest node into the path
            path.push_back(closest_node);
            prev_node = closest_node;
            //remove the inserted node from available stops
            available_stops[closest_index] = available_stops.back();
            available_stops.pop_back();
//          path_index++;
            updateAvailableStops(closest_node, available_stops,infos);
        }
        path.push_back(depot);
        return path;
}

void findDepotsCloseToPickUp(const std::vector<IntersectionIdx>& depots,const std::vector<IntersectionIdx>& pick_ups,IntersectionIdx & closest_depot, IntersectionIdx& second_closest,const std::vector<std::vector<OneRoute>>& routes_matrix,const std::unordered_map<IntersectionIdx, int>& intersection_to_index) {
    double global_fastest_pick = DBL_MAX;
    double global_second_fastest_pick = DBL_MAX;
    for (const auto &depot: depots) {
        double depot_fastest_pick = DBL_MAX;
        // find depot close to pick up
        for (auto node: pick_ups) {
            double current_time = routes_matrix.at(intersection_to_index.at(depot)).at(intersection_to_index.at(node)).travel_time;
            if (current_time < depot_fastest_pick) {
                depot_fastest_pick = current_time;
            }
        }
        if (depot_fastest_pick < global_fastest_pick) {
            global_second_fastest_pick = global_fastest_pick; // Update second fastest before updating the fastest
            global_fastest_pick = depot_fastest_pick;
            second_closest = closest_depot; // Update second closest
            closest_depot = depot;
        } else if (depot_fastest_pick < global_second_fastest_pick) {
            global_second_fastest_pick = depot_fastest_pick; // Update second fastest pick time
            second_closest = depot; // Update second closest depot for pickup
        }

    }
}

void findDepotsCloseToDropOff(const std::vector<IntersectionIdx>& depots,const std::vector<IntersectionIdx>& drop_offs,IntersectionIdx & close_drop,IntersectionIdx&second_close_drop,const std::vector<std::vector<OneRoute>>& routes_matrix,const std::unordered_map<IntersectionIdx, int>& intersection_to_index) {
    double global_fastest_drop = DBL_MAX;
    double global_second_fastest_drop = DBL_MAX; // New variable for second closest pickup
    for (const auto &depot: depots) {
        double depot_fastest_drop = DBL_MAX;
        for (auto node: drop_offs) {
            double current_time = routes_matrix.at(intersection_to_index.at(depot)).at(intersection_to_index.at(node)).travel_time;
            if (current_time < depot_fastest_drop) {
                depot_fastest_drop = current_time;
            }
        }
        if (depot_fastest_drop < global_fastest_drop) {
            global_second_fastest_drop = global_fastest_drop; // Update second fastest before updating the fastest
            global_fastest_drop = depot_fastest_drop;
            second_close_drop = close_drop; // Update second closest
            close_drop = depot;
        } else if (depot_fastest_drop < global_second_fastest_drop) {
            global_second_fastest_drop = depot_fastest_drop; // Update second fastest drop time
            second_close_drop = depot; // Update second closest depot for dropoff
        }
    }
}


std::vector<IntersectionIdx> swapAndShift(std::vector<IntersectionIdx>& path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info){
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_int_distribution<int> get_rand(1,path.size()-3);
    IntersectionIdx start = get_rand(generator);
    IntersectionIdx end =  std::min(start+3, static_cast<int>(path.size()) - 2);
    std::vector<IntersectionIdx> new_path =path;
    IntersectionIdx temp1 = new_path[end];
    IntersectionIdx temp2 =  new_path[end-1];
    IntersectionIdx temp3 =  new_path[end-2];
    IntersectionIdx temp4 =  new_path[end-3];
    std::uniform_int_distribution<int> rand_2(0,1);
    if(rand_2(generator)==0) {
        new_path[end] = temp2;
        new_path[end - 1] = temp3;
        new_path[end - 2] = temp4;
        new_path[end - 3] = temp1;
    }
    else{
        new_path[end-3]=temp3;
        new_path[end-2]=temp2;
        new_path[end-1]=temp1;
        new_path[end]=temp4;
    }
    if(!checkLegalNodeParallel(new_path,delivery_info)){
        return path;
    }
    else{
        return new_path;
    }
}


