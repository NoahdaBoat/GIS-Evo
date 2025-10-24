#include "converter/converter.hpp"

#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/io/writer.hpp>
#include <osmium/io/pbf_output.hpp>
#include <osmium/io/xml_input.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gisevo::converter {

int run_xml_to_pbf_converter(const XmlToPbfConfig& config) {
  if (config.input_xml.empty()) {
    std::cerr << "[xml_to_pbf_converter] Missing input XML file" << std::endl;
    return 1;
  }

  if (config.output_pbf.empty()) {
    std::cerr << "[xml_to_pbf_converter] Missing output PBF file" << std::endl;
    return 1;
  }

  if (!fs::exists(config.input_xml)) {
    std::cerr << "[xml_to_pbf_converter] Input file does not exist: " << config.input_xml << std::endl;
    return 1;
  }

  // Create output directory if it doesn't exist
  try {
    fs::create_directories(config.output_pbf.parent_path());
  } catch (const std::exception& ex) {
    std::cerr << "[xml_to_pbf_converter] Failed to create output directory: " << ex.what() << std::endl;
    return 1;
  }

  if (!config.quiet) {
    std::cout << "[xml_to_pbf_converter] Converting " << config.input_xml << " -> " << config.output_pbf << std::endl;
  }

  try {
    // Create input reader for XML file
    osmium::io::Reader reader{config.input_xml, osmium::osm_entity_bits::all};
    
    // Create output writer for PBF file
    osmium::io::Writer writer{config.output_pbf, osmium::io::overwrite::allow};
    
    // Copy all data from input to output
    while (osmium::memory::Buffer buffer = reader.read()) {
      writer(std::move(buffer));
    }
    
    // Close both reader and writer
    reader.close();
    writer.close();
    
    if (!config.quiet) {
      std::cout << "[xml_to_pbf_converter] Conversion completed successfully" << std::endl;
    }
    
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "[xml_to_pbf_converter] Conversion failed: " << ex.what() << std::endl;
    return 1;
  }
}

}  // namespace gisevo::converter
