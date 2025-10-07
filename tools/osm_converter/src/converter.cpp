#include "converter/converter.hpp"

#include "converter/schema.hpp"

#include <osmium/io/pbf_input.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/tag.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/visitor.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace gisevo::converter {
namespace {

using osm_id = osmium::object_id_type;

struct ConverterDataInternal {
  ConverterData data;
  std::unordered_set<osm_id> referenced_nodes;
  std::unordered_map<osm_id, std::size_t> node_index_lookup;
  std::vector<osm_id> missing_node_ids;
};

std::string to_lower_copy(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

HighwayCategory encode_highway_category(const char* value) {
  if (value == nullptr) {
    return HighwayCategory::kUnknown;
  }
  const std::string lower = to_lower_copy(value);
  if (lower == "motorway") return HighwayCategory::kMotorway;
  if (lower == "motorway_link") return HighwayCategory::kMotorway;
  if (lower == "trunk") return HighwayCategory::kTrunk;
  if (lower == "trunk_link") return HighwayCategory::kTrunk;
  if (lower == "primary") return HighwayCategory::kPrimary;
  if (lower == "primary_link") return HighwayCategory::kPrimary;
  if (lower == "secondary") return HighwayCategory::kSecondary;
  if (lower == "secondary_link") return HighwayCategory::kSecondary;
  if (lower == "tertiary") return HighwayCategory::kTertiary;
  if (lower == "tertiary_link") return HighwayCategory::kTertiary;
  if (lower == "residential") return HighwayCategory::kResidential;
  if (lower == "living_street") return HighwayCategory::kResidential;
  if (lower == "service") return HighwayCategory::kService;
  if (lower == "track") return HighwayCategory::kTrack;
  if (lower == "footway") return HighwayCategory::kFootway;
  if (lower == "pedestrian") return HighwayCategory::kFootway;
  if (lower == "path") return HighwayCategory::kPath;
  if (lower == "cycleway") return HighwayCategory::kCycleway;
  return HighwayCategory::kUnknown;
}

float parse_max_speed(const osmium::TagList& tags) {
  const char* raw_value = tags.get_value_by_key("maxspeed");
  if (!raw_value) {
    return -1.0F;
  }
  std::string value = raw_value;
  value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
  bool mph = false;
  if (value.size() > 3) {
    const std::string suffix = to_lower_copy(std::string_view(value).substr(value.size() - 3));
    if (suffix == "mph") {
      mph = true;
      value.resize(value.size() - 3);
    }
  }
  try {
    const float numeric = std::stof(value);
    if (mph) {
      return numeric * 1.60934F;
    }
    return numeric;
  } catch (const std::exception&) {
    return -1.0F;
  }
}

std::optional<std::string> detect_poi_category(const osmium::TagList& tags) {
  if (const char* amenity = tags.get_value_by_key("amenity")) {
    return std::string("amenity:") + amenity;
  }
  if (const char* shop = tags.get_value_by_key("shop")) {
    return std::string("shop:") + shop;
  }
  if (const char* tourism = tags.get_value_by_key("tourism")) {
    return std::string("tourism:") + tourism;
  }
  if (const char* leisure = tags.get_value_by_key("leisure")) {
    return std::string("leisure:") + leisure;
  }
  if (const char* railway = tags.get_value_by_key("railway")) {
    return std::string("railway:") + railway;
  }
  if (const char* public_transport = tags.get_value_by_key("public_transport")) {
    return std::string("public_transport:") + public_transport;
  }
  if (const char* highway = tags.get_value_by_key("highway")) {
    const std::string lower = to_lower_copy(highway);
    if (lower == "bus_stop" || lower == "tram_stop" || lower == "platform") {
      return std::string("highway:") + lower;
    }
  }
  if (const char* aeroway = tags.get_value_by_key("aeroway")) {
    return std::string("aeroway:") + aeroway;
  }
  return std::nullopt;
}

class HighwayCollector final : public osmium::handler::Handler {
 public:
  explicit HighwayCollector(ConverterDataInternal& internal)
    : internal_(internal) {}

  void way(const osmium::Way& way) {
    const char* highway = way.tags().get_value_by_key("highway");
    if (!highway) {
      return;
    }

    StreetSegmentRecord record;
    record.osm_id = way.id();
    record.category = encode_highway_category(highway);
    record.max_speed_kph = parse_max_speed(way.tags());

    if (const char* name = way.tags().get_value_by_key("name")) {
      record.name = name;
    }

    for (const auto& node_ref : way.nodes()) {
      internal_.referenced_nodes.insert(node_ref.ref());
      record.node_refs.push_back(node_ref.ref());
    }

    if (record.node_refs.size() < 2) {
      return;
    }

    internal_.data.street_segments.emplace_back(std::move(record));
  }

 private:
  ConverterDataInternal& internal_;
};

class NodeCollector final : public osmium::handler::Handler {
 public:
  explicit NodeCollector(ConverterDataInternal& internal)
      : internal_(internal) {}

  void node(const osmium::Node& node) {
    if (!node.location().valid()) {
      return;
    }

    const osm_id id = node.id();

    if (internal_.referenced_nodes.contains(id)) {
      NodeRecord record;
      record.osm_id = id;
      record.lat = node.location().lat();
      record.lon = node.location().lon();

      auto [iter, inserted] = internal_.node_index_lookup.emplace(id, internal_.data.nodes.size());
      if (inserted) {
        internal_.data.nodes.emplace_back(record);
      }
    }

    if (auto poi_category = detect_poi_category(node.tags())) {
      PoiRecord poi;
      poi.osm_id = id;
      poi.lat = node.location().lat();
      poi.lon = node.location().lon();
      poi.category = std::move(*poi_category);
      if (const char* name = node.tags().get_value_by_key("name")) {
        poi.name = name;
      }
      internal_.data.pois.emplace_back(std::move(poi));
    }
  }

 private:
  ConverterDataInternal& internal_;
};

class WayNodeValidator final : public osmium::handler::Handler {
 public:
  explicit WayNodeValidator(ConverterDataInternal& internal)
      : internal_(internal) {}

  void way(const osmium::Way& way) {
  if (!way.tags().has_key("highway")) {
      return;
    }

    for (const auto& node_ref : way.nodes()) {
      if (!internal_.node_index_lookup.contains(node_ref.ref())) {
        internal_.missing_node_ids.push_back(node_ref.ref());
      }
    }
  }

 private:
  ConverterDataInternal& internal_;
};

template <typename T>
void write_pod(std::ofstream& out, const T& value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void write_string(std::ofstream& out, const std::string& value) {
  const std::uint32_t length = static_cast<std::uint32_t>(value.size());
  write_pod(out, length);
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

void write_node_refs(std::ofstream& out, const std::vector<std::int64_t>& refs) {
  const std::uint32_t count = static_cast<std::uint32_t>(refs.size());
  write_pod(out, count);
  if (!refs.empty()) {
    out.write(reinterpret_cast<const char*>(refs.data()),
              static_cast<std::streamsize>(refs.size() * sizeof(std::int64_t)));
  }
}

void write_streets_file(const ConverterDataInternal& internal, const fs::path& output_file) {
  std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Failed to open streets output file: " + output_file.string());
  }

  out.write(kStreetsMagic, sizeof(kStreetsMagic));
  write_pod(out, kSchemaVersion);

  const std::uint64_t node_count = internal.data.nodes.size();
  const std::uint64_t segment_count = internal.data.street_segments.size();
  write_pod(out, node_count);
  write_pod(out, segment_count);

  for (const auto& node : internal.data.nodes) {
    write_pod(out, node.osm_id);
    write_pod(out, node.lat);
    write_pod(out, node.lon);
  }

  for (const auto& segment : internal.data.street_segments) {
    write_pod(out, segment.osm_id);
    const std::uint8_t category = static_cast<std::uint8_t>(segment.category);
    write_pod(out, category);
    write_pod(out, segment.max_speed_kph);
    write_string(out, segment.name);
    write_node_refs(out, segment.node_refs);
  }
}

void write_osm_file(const ConverterDataInternal& internal, const fs::path& output_file) {
  std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Failed to open OSM output file: " + output_file.string());
  }

  out.write(kOsmMagic, sizeof(kOsmMagic));
  write_pod(out, kSchemaVersion);

  const std::uint64_t poi_count = internal.data.pois.size();
  write_pod(out, poi_count);

  for (const auto& poi : internal.data.pois) {
    write_pod(out, poi.osm_id);
    write_pod(out, poi.lat);
    write_pod(out, poi.lon);
    write_string(out, poi.category);
    write_string(out, poi.name);
  }
}

ConverterDataInternal build_dataset(const fs::path& input, bool quiet) {
  ConverterDataInternal internal;

  {
    osmium::io::Reader way_reader{input, osmium::osm_entity_bits::way};
    HighwayCollector highway_handler{internal};
    osmium::apply(way_reader, highway_handler);
    way_reader.close();
  }

  {
    osmium::io::Reader node_reader{input, osmium::osm_entity_bits::node};
    NodeCollector node_handler{internal};
    osmium::apply(node_reader, node_handler);
    node_reader.close();
  }

  {
    osmium::io::Reader validator_reader{input, osmium::osm_entity_bits::way};
    WayNodeValidator validator{internal};
    osmium::apply(validator_reader, validator);
    validator_reader.close();
  }

  if (!internal.missing_node_ids.empty() && !quiet) {
    std::sort(internal.missing_node_ids.begin(), internal.missing_node_ids.end());
    internal.missing_node_ids.erase(
        std::unique(internal.missing_node_ids.begin(), internal.missing_node_ids.end()),
        internal.missing_node_ids.end());
    std::cerr << "Warning: missing " << internal.missing_node_ids.size()
              << " node locations referenced by highway ways." << std::endl;
  }

  return internal;
}

}  // namespace

int run_converter(const ConverterConfig& config) {
  if (config.input_pbf.empty()) {
    std::cerr << "[converter] Missing --input argument" << std::endl;
    return 1;
  }

  if (!fs::exists(config.input_pbf)) {
    std::cerr << "[converter] Input file does not exist: " << config.input_pbf << std::endl;
    return 1;
  }

  std::string map_name = config.map_name;
  if (map_name.empty()) {
    map_name = config.input_pbf.stem().string();
  }

  fs::path output_dir = config.output_directory;
  if (output_dir.empty()) {
    output_dir = fs::current_path();
  }

  const fs::path streets_path = output_dir / (map_name + ".streets.bin");
  const fs::path osm_path = output_dir / (map_name + ".osm.bin");

  const bool both_exist = fs::exists(streets_path) && fs::exists(osm_path);
  if (both_exist && !config.force_rebuild) {
    if (!config.quiet) {
      std::cout << "[converter] Existing binaries found; skipping conversion for " << map_name
                << std::endl;
    }
    return 0;
  }

  try {
    if (!output_dir.empty()) {
      fs::create_directories(output_dir);
    }
  } catch (const std::exception& ex) {
    std::cerr << "[converter] Failed to create output directory: " << ex.what() << std::endl;
    return 1;
  }

  if (!config.quiet) {
    std::cout << "[converter] Converting " << config.input_pbf << " -> " << streets_path
              << " / " << osm_path << std::endl;
  }

  const auto start_time = std::chrono::steady_clock::now();

  ConverterDataInternal internal;
  try {
    internal = build_dataset(config.input_pbf, config.quiet);
    write_streets_file(internal, streets_path);
    write_osm_file(internal, osm_path);
  } catch (const std::exception& ex) {
    std::cerr << "[converter] Conversion failed: " << ex.what() << std::endl;
    return 1;
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time);

  if (!config.quiet) {
    std::cout << "[converter] Wrote " << internal.data.nodes.size() << " nodes, "
              << internal.data.street_segments.size() << " street segments, "
              << internal.data.pois.size() << " POIs in " << elapsed.count() << "ms"
              << std::endl;
  }

  return 0;
}

}  // namespace gisevo::converter
