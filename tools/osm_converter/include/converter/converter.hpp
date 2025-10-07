#pragma once

#include <filesystem>
#include <string>

namespace gisevo::converter {

struct ConverterConfig {
  std::filesystem::path input_pbf;
  std::filesystem::path output_directory;
  std::string map_name;
  bool force_rebuild = false;
  bool quiet = false;
};

int run_converter(const ConverterConfig& config);

}  // namespace gisevo::converter
