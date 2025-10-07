//
// Created by helanlin on 4/7/24.
//
#include "m4.h"
#include "StreetsDatabaseAPI.h"
#include "globals.h"
#include "ms4helpers.hpp"
#include "struct.h"
#include <omp.h>


#include <vector>
#include <unordered_set>
#include <chrono>
#include <stdlib.h>
#include <thread>
#include <future>

// This routine takes in a vector of D deliveries (pickUp, dropOff
// intersection pairs), another vector of N intersections that
// are legal start and end points for the path (depots), and a turn
// penalty in seconds (see m3.h for details on turn penalties).
//
// The first vector 'deliveries' gives the delivery information.  Each delivery
// in this vector has pickUp and dropOff intersection ids.
// A delivery can only be dropped-off after the associated item has been picked-up.
//
// The second vector 'depots' gives the intersection ids of courier company
// depots containing trucks; you start at any one of these depots but you must
// end at the same depot you started at.
//
// This routine returns a vector of CourierSubPath objects that form a delivery route.
// The CourierSubPath is as defined above. The first street segment id in the
// first subpath is connected to a depot intersection, and the last street
// segment id of the last subpath also connects to a depot intersection.
// A package will not be dropped off if you haven't picked it up yet.
//
// The start_intersection of each subpath in the returned vector should be
// at least one of the following (a pick-up and/or drop-off can only happen at
// the start_intersection of a CourierSubPath object):
//      1- A start depot.
//      2- A pick-up location
//      3- A drop-off location.
//
// You can assume that D is always at least one and N is always at least one
// (i.e. both input vectors are non-empty).
//
// It is legal for the same intersection to appear multiple times in the pickUp
// or dropOff list (e.g. you might have two deliveries with a pickUp
// intersection id of #50). The same intersection can also appear as both a
// pickUp location and a dropOff location.
//
// If you have two pickUps to make at an intersection, traversing the
// intersection once is sufficient to pick up both packages. Additionally,
// one traversal of an intersection is sufficient to drop off all the
// (already picked up) packages that need to be dropped off at that intersection.
//
// Depots will never appear as pickUp or dropOff locations for deliveries.
//
// If no valid route to make *all* the deliveries exists, this routine must
// return an empty (size == 0) vector.

std::vector<CourierSubPath> travelingCourier(const float turn_penalty, const std::vector<DeliveryInf>& deliveries, const std::vector<IntersectionIdx>& depots) {

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<IntersectionIdx> pick_ups;
    std::vector<IntersectionIdx> drop_offs;
    std::vector<IntersectionIdx> key_intersections;
    loadDeliveryDetails(deliveries, depots, pick_ups, drop_offs, key_intersections);
    std::vector<CourierSubPath> best_delivery_route;
    std::vector<std::vector<OneRoute>> routes_matrix;


    // resize the routes matrix
    routes_matrix.resize(key_intersections.size());
    routes_matrix.resize(getNumIntersections());
    for (auto &i: routes_matrix) {
        i.resize(key_intersections.size());
    }

    const std::vector<IntersectionIdx> no_dup_deliveries = find_unique_intersections(deliveries, depots);
    // create an unordered map that contains a relation between the intersection index, and it's apparent position inside the key_intersections array
    std::unordered_map<IntersectionIdx, int> intersection_to_index;

    // call the function to form the unordered map
    preloadKeys(key_intersections, intersection_to_index);

    // call the multi-dijkstra algorithm to find the routes between all deliveries and depots

    compute_all_travel_times(key_intersections, intersection_to_index, routes_matrix, turn_penalty);


    // Output the result
    //std::cout << "Execution time: " << duration.count() << " milliseconds" << std::endl;

    // next, find a legal beginning path

    std::vector<double> depot_travel_time;
    std::vector<std::vector<IntersectionIdx>> vec_path;
    std::vector<IntersectionIdx> closest_depots;

    // if less than 8 depots, run greedy on all of them
    if(depots.size()>8) {
        vec_path.resize(4);
        depot_travel_time.resize(4);

        IntersectionIdx closest_depot = depots[0];
        IntersectionIdx second_closest =
                depots.size() > 1 ? depots[1] : depots[0]; // Initialize to the second depot, if available
        IntersectionIdx close_drop = depots[0];
        IntersectionIdx second_close_drop = depots.size() > 1 ? depots[1] : depots[0]; // Initialize similarly
        std::thread t1(findDepotsCloseToPickUp,std::cref(depots),std::cref(pick_ups),std::ref(closest_depot),std::ref(second_closest),std::cref(routes_matrix),std::cref(intersection_to_index));
        findDepotsCloseToDropOff(depots,drop_offs,close_drop,second_close_drop,routes_matrix,intersection_to_index);
        t1.join();

        closest_depots = {closest_depot, close_drop, second_close_drop, second_closest};
    }
    else{
        vec_path.resize(depots.size());
        depot_travel_time.resize(depots.size());
        closest_depots = depots;
    }
    #pragma omp parallel for
    for(int i = 0; i < closest_depots.size(); i++){
        vec_path[i]= greedyAlgo(pick_ups,routes_matrix,closest_depots[i],intersection_to_index);
        depot_travel_time[i] = pathCost(vec_path[i],routes_matrix,intersection_to_index);
    }

    // find initial path with the shortest travel time
    auto fastest_time = DBL_MAX;
    int depot_index = 0;
    for(int i = 0; i < closest_depots.size(); i++){
        if(depot_travel_time[i] < fastest_time){
            fastest_time = depot_travel_time[i];
            depot_index = i;
        }
    }
    std::vector<IntersectionIdx> path = vec_path[depot_index];

    // now call our algorithm that tests different routes
    if(deliveries.size()>20) {
        const double initial_path_cost = pathCost(path, routes_matrix, intersection_to_index);
        std::cout << initial_path_cost << std::endl;
        int temperature = 150;
        const double alpha = 0.99;
        const std::unordered_map<IntersectionIdx, Delivery_details> delivery_details = globals.delivery_info;

        unsigned int seed = time(nullptr) ^ omp_get_thread_num();
        struct drand48_data buffer;
        srand48_r(seed, &buffer);
        std::vector<IntersectionIdx> best_path = path;
        std::vector<std::future<std::vector<IntersectionIdx>>> futures;
        uint num_parts = std::thread::hardware_concurrency() / 2;
        double curr_cost = initial_path_cost;

        int midpoint = (int)(path.size() / 2); // Index at midpoint
        int firstHalfPerThread = midpoint / 2;
        int secondHalfPerThread = (path.size() - midpoint) / 6;

        std::vector<int> parts(num_parts, 0);
        parts[0] = firstHalfPerThread;
        parts[1]= midpoint -firstHalfPerThread;
        for (int i = 2; i < num_parts; ++i) {
            parts[i] = secondHalfPerThread;
            if (i == num_parts - 1) {
                parts[i] = path.size() - midpoint - 5 * secondHalfPerThread; // Ensure all tasks in the second half are covered
            }
        }

        // Dispatch tasks
        int current_start = 0;
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        const long time_to_here = duration.count();
        for (int i = 0; i < num_parts; ++i) {
            if (parts[i] > 0) {
                futures.push_back(std::async(std::launch::async, annealingTwoOpt, temperature, alpha, buffer,std::cref(path),initial_path_cost,
                                             std::ref(routes_matrix), time_to_here, std::ref(intersection_to_index),
                                             delivery_details, current_start + parts[i] + 1, current_start + 1));
            }
            current_start += parts[i];
        }
        for (auto &future: futures) {
            std::vector<IntersectionIdx> final_path = future.get();
            double final_cost = pathCost(final_path, routes_matrix, intersection_to_index);
            if (final_cost < curr_cost) {
                curr_cost = final_cost;
                best_path = final_path;
            }
        }

        best_delivery_route = indexToSubPath(best_path, routes_matrix, intersection_to_index);

    }
    else{
        best_delivery_route = indexToSubPath(path, routes_matrix, intersection_to_index);
    }

    globals.delivery_info.clear();
    return best_delivery_route;
}