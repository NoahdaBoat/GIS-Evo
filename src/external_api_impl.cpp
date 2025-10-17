#include "StreetsDatabaseAPI.h"
#include "OSMDatabaseAPI.h"
#include "binary_loader/binary_database.hpp"
#include <algorithm>
#include <iostream>

using namespace gisevo;

// OSM Wrapper implementations
struct OSMNodeImpl {
    std::size_t index_;
    
    OSMNodeImpl() : index_(0) {}
    OSMNodeImpl(std::size_t index) : index_(index) {}
    
    OSMID id() const { 
        auto& db = BinaryDatabase::instance();
        return db.get_node_osm_id(index_);
    }
    LatLon coords() const { 
        auto& db = BinaryDatabase::instance();
        return db.get_node_position(index_);
    }
};

struct OSMWayImpl {
    std::size_t index_;
    
    OSMWayImpl() : index_(0) {}
    OSMWayImpl(std::size_t index) : index_(index) {}
    
    OSMID id() const { 
        auto& db = BinaryDatabase::instance();
        return db.get_segment_info(index_).wayOSMID;
    }
    bool isClosed() const { 
        auto& db = BinaryDatabase::instance();
        const auto& node_refs = db.get_segment_node_refs(index_);
        if (node_refs.size() < 2) return false;
        return node_refs.front() == node_refs.back();
    }
};

struct OSMRelationImpl {
    std::size_t index_;
    
    OSMRelationImpl() : index_(0) {}
    OSMRelationImpl(std::size_t index) : index_(index) {}
    
    OSMID id() const { 
        auto& db = BinaryDatabase::instance();
        return db.get_relation_by_index(index_).osm_id;
    }
};

// Database loading
bool loadStreetsDatabaseBIN(std::string path) {
    return BinaryDatabase::instance().load_streets_file(path);
}

void closeStreetDatabase() {
    BinaryDatabase::instance().clear();
}

bool loadOSMDatabaseBIN(std::string path) {
    return BinaryDatabase::instance().load_osm_file(path);
}

void closeOSMDatabase() {
    // OSM data cleared with streets database
}

// Street segment API
unsigned getNumStreetSegments() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_segment_count());
}

StreetSegmentInfo getStreetSegmentInfo(StreetSegmentIdx segment_idx) {
    return BinaryDatabase::instance().get_segment_info(static_cast<std::size_t>(segment_idx));
}

unsigned getStreetSegmentCurvePointCount(StreetSegmentIdx segment_idx) {
    auto& db = BinaryDatabase::instance();
    if (segment_idx >= db.get_segment_count()) {
        return 0;
    }
    
    auto segment_info = db.get_segment_info(static_cast<std::size_t>(segment_idx));
    return segment_info.numCurvePoints;
}

LatLon getStreetSegmentCurvePoint(unsigned point_idx, StreetSegmentIdx segment_idx) {
    auto& db = BinaryDatabase::instance();
    if (segment_idx >= db.get_segment_count()) {
        return LatLon(0, 0);
    }
    
    auto segment_info = db.get_segment_info(static_cast<std::size_t>(segment_idx));
    if (point_idx >= segment_info.numCurvePoints) {
        return LatLon(0, 0);
    }
    
    // For now, return dummy curve points between intersections
    // TODO: Implement proper curve point calculation from OSM data
    LatLon from_pos = db.get_intersection_position(static_cast<std::size_t>(segment_info.from));
    LatLon to_pos = db.get_intersection_position(static_cast<std::size_t>(segment_info.to));
    
    // Calculate intermediate points along the line
    double t = static_cast<double>(point_idx + 1) / static_cast<double>(segment_info.numCurvePoints + 1);
    double lat = from_pos.latitude() + t * (to_pos.latitude() - from_pos.latitude());
    double lon = from_pos.longitude() + t * (to_pos.longitude() - from_pos.longitude());
    
    return LatLon(lat, lon);
}

// Street API
unsigned getNumStreets() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_street_count());
}

std::string getStreetName(StreetIdx street_idx) {
    return BinaryDatabase::instance().get_street_name(static_cast<std::size_t>(street_idx));
}

// Intersection API
unsigned getNumIntersections() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_intersection_count());
}

LatLon getIntersectionPosition(IntersectionIdx intersection_idx) {
    return BinaryDatabase::instance().get_intersection_position(static_cast<std::size_t>(intersection_idx));
}

std::string getIntersectionName(IntersectionIdx intersection_idx) {
    return BinaryDatabase::instance().get_intersection_name(static_cast<std::size_t>(intersection_idx));
}

OSMID getIntersectionOSMNodeID(IntersectionIdx intersection_idx) {
    return BinaryDatabase::instance().get_intersection_osm_node_id(static_cast<std::size_t>(intersection_idx));
}

// Intersection street segment functions
unsigned getNumIntersectionStreetSegment(IntersectionIdx intersection_idx) {
    return BinaryDatabase::instance().get_intersection_street_segment_count(static_cast<std::size_t>(intersection_idx));
}

StreetSegmentIdx getIntersectionStreetSegment(unsigned street_segment_num, IntersectionIdx intersection_idx) {
    return BinaryDatabase::instance().get_intersection_street_segment(street_segment_num, static_cast<std::size_t>(intersection_idx));
}

// POI API
unsigned getNumPointsOfInterest() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_poi_count());
}

LatLon getPOIPosition(POIIdx poi_idx) {
    return BinaryDatabase::instance().get_poi_position(static_cast<std::size_t>(poi_idx));
}

std::string getPOIName(POIIdx poi_idx) {
    return BinaryDatabase::instance().get_poi_name(static_cast<std::size_t>(poi_idx));
}

std::string getPOIType(POIIdx poi_idx) {
    return BinaryDatabase::instance().get_poi_type(static_cast<std::size_t>(poi_idx));
}

// Node API
unsigned getNumberOfNodes() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_node_count());
}

const OSMNode* getNodeByIndex(unsigned node_idx) {
    auto& db = BinaryDatabase::instance();
    if (node_idx >= db.get_node_count()) {
        return nullptr;
    }
    
    // Create a static OSMNode wrapper - this is a bit of a hack but necessary for C API compatibility
    static OSMNode node_wrapper;
    
    // Cast the stub to our implementation to store data
    auto* impl = reinterpret_cast<OSMNodeImpl*>(&node_wrapper);
    *impl = OSMNodeImpl(node_idx);
    
    return &node_wrapper;
}

LatLon getNodeCoords(const OSMNode* node) {
    if (!node) {
        return LatLon(0, 0);
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMNodeImpl*>(node);
    return impl->coords();
}

unsigned getTagCount(const OSMNode* node) {
    if (!node) {
        return 0;
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMNodeImpl*>(node);
    auto& db = BinaryDatabase::instance();
    return static_cast<unsigned>(db.get_node_tags(impl->index_).size());
}

std::pair<std::string, std::string> getTagPair(const OSMNode* node, unsigned tag_idx) {
    if (!node) {
        return {"", ""};
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMNodeImpl*>(node);
    auto& db = BinaryDatabase::instance();
    const auto& tags = db.get_node_tags(impl->index_);
    if (tag_idx >= tags.size()) {
        return {"", ""};
    }
    return tags[tag_idx];
}

// Way API
unsigned getNumberOfWays() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_segment_count());
}

const OSMWay* getWayByIndex(unsigned way_idx) {
    auto& db = BinaryDatabase::instance();
    if (way_idx >= db.get_segment_count()) {
        return nullptr;
    }
    
    // Create a static OSMWay wrapper
    static OSMWay way_wrapper;
    
    // Cast the stub to our implementation to store data
    auto* impl = reinterpret_cast<OSMWayImpl*>(&way_wrapper);
    *impl = OSMWayImpl(way_idx);
    
    return &way_wrapper;
}

unsigned getTagCount(const OSMWay* way) {
    if (!way) {
        return 0;
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMWayImpl*>(way);
    auto& db = BinaryDatabase::instance();
    return static_cast<unsigned>(db.get_segment_tags(impl->index_).size());
}

std::pair<std::string, std::string> getTagPair(const OSMWay* way, unsigned tag_idx) {
    if (!way) {
        return {"", ""};
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMWayImpl*>(way);
    auto& db = BinaryDatabase::instance();
    const auto& tags = db.get_segment_tags(impl->index_);
    if (tag_idx >= tags.size()) {
        return {"", ""};
    }
    return tags[tag_idx];
}

bool isClosedWay(const OSMWay* way) {
    if (!way) {
        return false;
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMWayImpl*>(way);
    return impl->isClosed();
}

// Relation API
unsigned getNumberOfRelations() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_relation_count());
}

const OSMRelation* getRelationByIndex(unsigned relation_idx) {
    auto& db = BinaryDatabase::instance();
    if (relation_idx >= db.get_relation_count()) {
        return nullptr;
    }
    
    // Create a static OSMRelation wrapper
    static OSMRelation relation_wrapper;
    
    // Cast the stub to our implementation to store data
    auto* impl = reinterpret_cast<OSMRelationImpl*>(&relation_wrapper);
    *impl = OSMRelationImpl(relation_idx);
    
    return &relation_wrapper;
}

unsigned getTagCount(const OSMRelation* relation) {
    if (!relation) {
        return 0;
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMRelationImpl*>(relation);
    auto& db = BinaryDatabase::instance();
    return static_cast<unsigned>(db.get_relation_tags(impl->index_).size());
}

std::pair<std::string, std::string> getTagPair(const OSMRelation* relation, unsigned tag_idx) {
    if (!relation) {
        return {"", ""};
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMRelationImpl*>(relation);
    auto& db = BinaryDatabase::instance();
    const auto& tags = db.get_relation_tags(impl->index_);
    if (tag_idx >= tags.size()) {
        return {"", ""};
    }
    return tags[tag_idx];
}

std::vector<std::string> getRelationMemberRoles(const OSMRelation* relation) {
    if (!relation) {
        return {};
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMRelationImpl*>(relation);
    auto& db = BinaryDatabase::instance();
    return db.get_relation_member_roles(impl->index_);
}

std::vector<TypedOSMID> getRelationMembers(const OSMRelation* relation) {
    if (!relation) {
        return {};
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMRelationImpl*>(relation);
    auto& db = BinaryDatabase::instance();
    
    const auto& member_ids = db.get_relation_member_ids(impl->index_);
    const auto& member_types = db.get_relation_member_types(impl->index_);
    
    std::vector<TypedOSMID> members;
    members.reserve(member_ids.size());
    
    for (std::size_t i = 0; i < member_ids.size(); ++i) {
        TypedOSMID::EntityType type;
        switch (member_types[i]) {
            case 0: type = TypedOSMID::Node; break;
            case 1: type = TypedOSMID::Way; break;
            case 2: type = TypedOSMID::Relation; break;
            default: type = TypedOSMID::Invalid; break;
        }
        members.emplace_back(member_ids[i], type);
    }
    
    return members;
}

// Feature API
unsigned getNumFeatures() {
    return static_cast<unsigned>(BinaryDatabase::instance().get_feature_count());
}

FeatureType getFeatureType(FeatureIdx idx) {
    return BinaryDatabase::instance().get_feature_type(static_cast<std::size_t>(idx));
}

OSMID getFeatureOSMID(FeatureIdx idx) {
    return BinaryDatabase::instance().get_feature_osm_id(static_cast<std::size_t>(idx));
}

std::string getFeatureName(FeatureIdx idx) {
    return BinaryDatabase::instance().get_feature_name(static_cast<std::size_t>(idx));
}

unsigned getNumFeaturePoints(FeatureIdx feature_idx) {
    return static_cast<unsigned>(BinaryDatabase::instance().get_feature_point_count(static_cast<std::size_t>(feature_idx)));
}

LatLon getFeaturePoint(unsigned point_idx, FeatureIdx feature_idx) {
    return BinaryDatabase::instance().get_feature_point(static_cast<std::size_t>(point_idx), static_cast<std::size_t>(feature_idx));
}

// OSM Helper functions
std::vector<OSMID> getWayNodes(OSMID way_id) {
    auto& db = BinaryDatabase::instance();
    const auto& way_lookup = db.get_way_id_to_segment_index();
    const auto it = way_lookup.find(way_id);
    if (it == way_lookup.end()) {
        return {};
    }
    
    const auto& segment = db.get_segment_by_index(it->second);
    return segment.node_refs;
}

TypedOSMID getNodeByOSMID(OSMID node_id) {
    auto& db = BinaryDatabase::instance();
    const auto& node_lookup = db.get_node_id_to_index();
    const auto it = node_lookup.find(node_id);
    if (it == node_lookup.end()) {
        return TypedOSMID(node_id, TypedOSMID::Invalid);
    }
    return TypedOSMID(node_id, TypedOSMID::Node);
}

std::vector<OSMID> getWayMembers(const OSMWay* way) {
    if (!way) {
        return {};
    }
    
    // Cast the stub to our implementation
    const auto* impl = reinterpret_cast<const OSMWayImpl*>(way);
    auto& db = BinaryDatabase::instance();
    return db.get_segment_node_refs(impl->index_);
}

// Legacy POI functions (duplicates)
unsigned getNumberOfPOIs() {
    return getNumPointsOfInterest();
}

// Spatial query API
std::vector<unsigned> queryStreetsInBounds(double min_x, double min_y, double max_x, double max_y) {
    auto results = BinaryDatabase::instance().query_streets_in_bounds(min_x, min_y, max_x, max_y);
    std::vector<unsigned> unsigned_results;
    unsigned_results.reserve(results.size());
    for (auto idx : results) {
        unsigned_results.push_back(static_cast<unsigned>(idx));
    }
    return unsigned_results;
}

std::vector<unsigned> queryIntersectionsInBounds(double min_x, double min_y, double max_x, double max_y) {
    auto results = BinaryDatabase::instance().query_intersections_in_bounds(min_x, min_y, max_x, max_y);
    std::vector<unsigned> unsigned_results;
    unsigned_results.reserve(results.size());
    for (auto idx : results) {
        unsigned_results.push_back(static_cast<unsigned>(idx));
    }
    return unsigned_results;
}

std::vector<unsigned> queryPOIsInBounds(double min_x, double min_y, double max_x, double max_y) {
    auto results = BinaryDatabase::instance().query_pois_in_bounds(min_x, min_y, max_x, max_y);
    std::vector<unsigned> unsigned_results;
    unsigned_results.reserve(results.size());
    for (auto idx : results) {
        unsigned_results.push_back(static_cast<unsigned>(idx));
    }
    return unsigned_results;
}

std::vector<unsigned> queryFeaturesInBounds(double min_x, double min_y, double max_x, double max_y) {
    auto results = BinaryDatabase::instance().query_features_in_bounds(min_x, min_y, max_x, max_y);
    std::vector<unsigned> unsigned_results;
    unsigned_results.reserve(results.size());
    for (auto idx : results) {
        unsigned_results.push_back(static_cast<unsigned>(idx));
    }
    return unsigned_results;
}

// Load map function (calls both loaders)
bool load_map(std::string map_name) {
    std::string streets_path = map_name + ".streets.bin";
    std::string osm_path = map_name + ".osm.bin";

    bool streets_loaded = loadStreetsDatabaseBIN(streets_path);
    bool osm_loaded = loadOSMDatabaseBIN(osm_path);

    return streets_loaded && osm_loaded;
}

void close_map() {
    closeStreetDatabase();
    closeOSMDatabase();
}