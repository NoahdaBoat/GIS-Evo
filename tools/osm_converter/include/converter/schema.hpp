#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gisevo::converter {

inline constexpr std::uint32_t kSchemaVersion = 2;
inline constexpr char kStreetsMagic[8] = {'G', 'I', 'S', 'E', 'V', 'O', 'S', '2'};
inline constexpr char kOsmMagic[8] = {'G', 'I', 'S', 'E', 'V', 'O', 'O', '2'};

struct NodeRecord {
  std::int64_t osm_id;
  double lat;
  double lon;
  std::vector<std::pair<std::string, std::string>> tags;
};

enum class HighwayCategory : std::uint8_t {
  kUnknown = 0,
  kMotorway,
  kTrunk,
  kPrimary,
  kSecondary,
  kTertiary,
  kResidential,
  kService,
  kTrack,
  kFootway,
  kPath,
  kCycleway,
};

struct StreetSegmentRecord {
  std::int64_t osm_id;
  HighwayCategory category;
  float max_speed_kph;
  std::string name;
  std::vector<std::int64_t> node_refs;
  std::vector<std::pair<std::string, std::string>> tags;
};

struct PoiRecord {
  std::int64_t osm_id;
  double lat;
  double lon;
  std::string category;
  std::string name;
  std::vector<std::pair<std::string, std::string>> tags;
};

enum class FeatureType : std::uint8_t {
  kUnknown = 0,
  kPark,
  kWater,
  kBuilding,
  kForest,
  kGrassland,
  kWetland,
  kBeach,
  kGarden,
  kPlayground,
  kCemetery,
  kHospital,
  kSchool,
  kUniversity,
  kStadium,
  kAirport,
  kRailway,
  kBridge,
  kTunnel,
  kWall,
  kFence,
  kBarrier,
  kCoastline,
  kRiver,
  kStream,
  kCanal,
  kLake,
  kPond,
  kReservoir,
  kBay,
  kSea,
  kOcean
};

struct FeatureRecord {
  std::int64_t osm_id;
  FeatureType type;
  std::string name;
  std::vector<std::int64_t> node_refs;  // For ways/relations
  bool is_closed;  // True if first and last points are the same
  std::vector<std::pair<std::string, std::string>> tags;
};

struct RelationRecord {
  std::int64_t osm_id;
  std::vector<std::pair<std::string, std::string>> tags;
  std::vector<std::int64_t> member_ids;
  std::vector<std::uint8_t> member_types;  // 0=Node, 1=Way, 2=Relation
  std::vector<std::string> member_roles;
};

struct ConverterData {
  std::vector<NodeRecord> nodes;
  std::vector<StreetSegmentRecord> street_segments;
  std::vector<PoiRecord> pois;
  std::vector<FeatureRecord> features;
  std::vector<RelationRecord> relations;
};

}  // namespace gisevo::converter
