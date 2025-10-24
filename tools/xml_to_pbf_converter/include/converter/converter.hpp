#pragma once

#include <filesystem>
#include <string>

namespace gisevo::converter {

struct XmlToPbfConfig {
  std::filesystem::path input_xml;
  std::filesystem::path output_pbf;
  bool quiet = false;
};

int run_xml_to_pbf_converter(const XmlToPbfConfig& config);

}  // namespace gisevo::converter
