#include "converter/converter.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void print_usage() {
  std::cout << "Usage: xml_to_pbf_converter --input <file.xml> --output <file.osm.pbf> [options]\n"
               "\n"
               "Options:\n"
               "  -i, --input <path>        Path to the source .xml file\n"
               "  -o, --output <path>       Path for the output .osm.pbf file\n"
               "  -q, --quiet               Suppress progress logging\n"
               "  -h, --help                Show this help text\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  gisevo::converter::XmlToPbfConfig config;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    }
    if (arg == "-i" || arg == "--input") {
      if (i + 1 >= argc) {
        std::cerr << "[xml_to_pbf_converter] Missing value for --input" << std::endl;
        return 1;
      }
      config.input_xml = fs::path(argv[++i]);
    } else if (arg == "-o" || arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "[xml_to_pbf_converter] Missing value for --output" << std::endl;
        return 1;
      }
      config.output_pbf = fs::path(argv[++i]);
    } else if (arg == "-q" || arg == "--quiet") {
      config.quiet = true;
    } else {
      std::cerr << "[xml_to_pbf_converter] Unrecognized argument: " << arg << "\n";
      print_usage();
      return 1;
    }
  }

  return gisevo::converter::run_xml_to_pbf_converter(config);
}
