#include "converter/converter.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void print_usage() {
  std::cout << "Usage: osm_converter --input <file.osm.pbf> [options]\n"
               "\n"
               "Options:\n"
               "  -i, --input <path>        Path to the source .osm.pbf file\n"
               "  -o, --output-dir <path>   Directory for output binaries (default: cwd)\n"
               "  -n, --map-name <name>     Base name for generated files (default: input stem)\n"
               "  -f, --force               Regenerate even if binaries already exist\n"
               "  -q, --quiet               Suppress progress logging\n"
               "  -h, --help                Show this help text\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  gisevo::converter::ConverterConfig config;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    }
    if (arg == "-i" || arg == "--input") {
      if (i + 1 >= argc) {
        std::cerr << "[converter] Missing value for --input" << std::endl;
        return 1;
      }
      config.input_pbf = fs::path(argv[++i]);
    } else if (arg == "-o" || arg == "--output-dir") {
      if (i + 1 >= argc) {
        std::cerr << "[converter] Missing value for --output-dir" << std::endl;
        return 1;
      }
      config.output_directory = fs::path(argv[++i]);
    } else if (arg == "-n" || arg == "--map-name") {
      if (i + 1 >= argc) {
        std::cerr << "[converter] Missing value for --map-name" << std::endl;
        return 1;
      }
      config.map_name = argv[++i];
    } else if (arg == "-f" || arg == "--force") {
      config.force_rebuild = true;
    } else if (arg == "-q" || arg == "--quiet") {
      config.quiet = true;
    } else {
      std::cerr << "[converter] Unrecognized argument: " << arg << "\n";
      print_usage();
      return 1;
    }
  }

  return gisevo::converter::run_converter(config);
}
