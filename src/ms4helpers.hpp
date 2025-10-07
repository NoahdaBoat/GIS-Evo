//
// Created by montinoa on 4/5/24.
//

#pragma once

#include "StreetsDatabaseAPI.h"
#include "m4.h"
#include "struct.h"
#include "sort_streetseg/streetsegment_info.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>

enum Stop_Type{
    DEPOT = 0,
    PICKUP,
    DROPOFF,
};

class OneRoute {
public:
    std::vector<StreetSegmentIdx> route;
    double travel_time;
    IntersectionIdx start;
    IntersectionIdx end;

    // Constructors
    OneRoute(std::vector<StreetSegmentIdx> i_route,
             double i_travel_time,
             IntersectionIdx i_start,
             IntersectionIdx i_end) {

        route = i_route;
        travel_time = i_travel_time;
        start = i_start;
        end = i_end;
    }
    OneRoute() = default;
};

void compute_all_travel_times(const std::vector<IntersectionIdx>& of_interest,
                              const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
                              std::vector<std::vector<OneRoute>>& route_matrix,
                              const float& turn_penalty);

std::vector<IntersectionIdx> find_unique_intersections(const std::vector<DeliveryInf> &deliveries, const std::vector<IntersectionIdx>& depots);

void multi_dijkstra(IntersectionIdx start,
                    std::vector<IntersectionIdx> of_interest,
                    float turn_penalty,
                    std::vector<std::vector<OneRoute>>& route_matrix,
                    std::unordered_map<IntersectionIdx, int> intersection_to_index,
                    std::vector<street_segment_info> local_all_segments);

void preloadDeliveryStops(const std::vector<DeliveryInf> &deliveries);

void clearVisitedNodes(std::vector<IntersectionIdx> &path, int index);

void preloadKeys(const std::vector<IntersectionIdx>& key_intersections,std::unordered_map<IntersectionIdx , int>& returning_map );

bool checkLegalNode(std::vector<IntersectionIdx> &path);

void loadDeliveryDetails(const std::vector<DeliveryInf> &deliveries,const std::vector<IntersectionIdx>&depots, std::vector<IntersectionIdx>& pick_ups, std::vector<IntersectionIdx>& drop_offs, std::vector<IntersectionIdx>& unique_intersections);

void updateAvailableStops(IntersectionIdx new_stop, std::vector<IntersectionIdx> &available_stops, std::unordered_map<IntersectionIdx ,Delivery_details>& infos);

std::vector<CourierSubPath> indexToSubPath(const std::vector<IntersectionIdx>& path, const std::vector<std::vector<OneRoute>>& routes_matrix, std::unordered_map<IntersectionIdx, int>& intersection_to_index);

std::vector<IntersectionIdx> simulatedAnnealing(int temperature,
                                                std::vector<IntersectionIdx> start_path,
                                                double start_path_cost,
                                                int num_perturbations,
                                                std::vector<std::vector<OneRoute>> routes_matrix,
                                                struct drand48_data buffer,
                                                double alpha,
                                                double time_taken,
                                                std::unordered_map<IntersectionIdx, int> intersection_to_index);

double pathCost(const std::vector<IntersectionIdx>& path, const std::vector<std::vector<OneRoute>>& routes_matrix, const std::unordered_map<IntersectionIdx, int>& intersection_to_index);

std::vector<IntersectionIdx> perturbationSwap(std::vector<IntersectionIdx>& path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info);

std::vector<IntersectionIdx> perturbationTwoOpt(std::vector<IntersectionIdx> path);

std::vector<IntersectionIdx> perturbeTravelRoute(std::vector<IntersectionIdx>& path);

std::vector<IntersectionIdx> perturbationMoveOne(std::vector<IntersectionIdx>& path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info);

int generateDistribution(const int& min, const int& max);

std::vector<IntersectionIdx> twoOptVTwo(std::vector<IntersectionIdx>& path, const int index_1, const int index_2 ,std::unordered_map<IntersectionIdx, Delivery_details> delivery_info);

std::vector<IntersectionIdx> twoOptImplementation (const std::vector<IntersectionIdx>& start_path,
                                                   const double start_path_cost,
                                                   const std::vector<std::vector<OneRoute>>& routes_matrix,
                                                   const long time_taken,
                                                   const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
                                                   const std::unordered_map<IntersectionIdx, Delivery_details> delivery_info,
                                                   const int max_bound,
                                                   const int min_bound);

std::vector<IntersectionIdx> simulatedAnnealingV2 (const std::vector<IntersectionIdx>& start_path,
                                                   const double start_path_cost,
                                                   const std::vector<std::vector<OneRoute>>& routes_matrix,
                                                   const long time_taken,
                                                   const std::unordered_map<IntersectionIdx, int>& intersection_to_index,
                                                   std::unordered_map<IntersectionIdx, Delivery_details> delivery_details,
                                                   const int max_bound,
                                                   const int min_bound);

bool checkLegalNodeParallel(const std::vector<IntersectionIdx> path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info);

std::vector<IntersectionIdx> greedyAlgo (std::vector<IntersectionIdx>& pick_ups,std::vector<std::vector<OneRoute>>& routes_matrix, IntersectionIdx depot, const std::unordered_map<IntersectionIdx, int>& intersection_to_index );

void findDepotsCloseToPickUp(const std::vector<IntersectionIdx>& depots,const std::vector<IntersectionIdx>& pick_ups,IntersectionIdx & closest_depot, IntersectionIdx& second_closest,const std::vector<std::vector<OneRoute>>& routes_matrix,const std::unordered_map<IntersectionIdx, int>& intersection_to_index);

void findDepotsCloseToDropOff(const std::vector<IntersectionIdx>& depots,const std::vector<IntersectionIdx>& drop_offs,IntersectionIdx & close_drop,IntersectionIdx&second_close_drop,const std::vector<std::vector<OneRoute>>& routes_matrix,const std::unordered_map<IntersectionIdx, int>& intersection_to_index);

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
                                                const int min_bound);

std::vector<IntersectionIdx> swapAndShift(std::vector<IntersectionIdx>& path, std::unordered_map<IntersectionIdx, Delivery_details> delivery_info);