#include "ui.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "GIS Evo - Clean Architecture Demo" << std::endl;
    
    gisevo::ui::Application app;
    
    // Try to load map data
    std::string streets_path = "/home/nmonti/GitHub/GIS-Evo/resources/maps/ontario.streets.bin";
    std::string osm_path = "/home/nmonti/GitHub/GIS-Evo/resources/maps/ontario.osm.bin";
    
    std::cout << "Loading map data..." << std::endl;
    bool loaded = app.load_map(streets_path, osm_path);
    
    if (loaded) {
        std::cout << "Map loaded successfully!" << std::endl;
    } else {
        std::cout << "Failed to load map data (binary loading not yet implemented)" << std::endl;
        std::cout << "This is expected - the new architecture is ready but needs binary loader implementation" << std::endl;
    }
    
    std::cout << "Starting application..." << std::endl;
    return app.run(argc, argv);
}
