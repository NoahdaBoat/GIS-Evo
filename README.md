# OpenStreetMap GIS
## Spring 2024
### Lanlin He, Nicole Jiao, Noah Monti
### ECE297, University of Toronto

### Overview
This software was written in C++ and uses GTK3.0 to display curated OSM data for several cities across the globe.

Users select a city and can view various layers such as transit, points of interest, and live data from FourSquare.

Roads are highlighted based on the selected level of detail, as well as the drawing and thickness of map features such as buildings. The map's colours can be inverted (light to dark and vice-versa) based on user preferences.

Selecting two interesections on the map will generate a travel path highlighted on the map, as well as turn-by-turn directions.

The codebase also contains a multi-threaded implementation of the Simulated Annealing algorithm, which can be used to generate a path between multiple stops on the map.

### Details
The software utilizes culling to increase performance by skipping the calculation and rendering of features not in the users current view.

To find the best route between two locations on the map, the A* algorithm was implemented.

Code was written to take advantage of libjsoncpp to parse the API data from FourSquare and display the most relevant POIs across the selected city, with additional information available such as phone numbers, ratings, and websites.

The algorithm to generate a route for multiple stops uses OpenMP to create an adjacency matrix between all stops, and to run the Multi-Dijkstra algorithm to find the distance (cost) between all stops. Next it performs several permutation algorithms (e.g. 3-OPT & 4-OPT) to find a sutible starting route that can be fine tuned. Next, using std::async, the Simulated Annealing algorithm is run to find the best route within a local minimum. The route between multiple depots was constrained to be completed in under one minute.
