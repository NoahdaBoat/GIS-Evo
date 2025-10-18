# Build configuration for GIS-Evo core library
# This is a compilation test target to check migrated files for errors

gtk_dep = dependency('gtk4', required: true)
cairo_dep = dependency('cairo', required: true)
threads_dep = dependency('threads', required: true)
tbb_dep = dependency('tbb', required: false)

# Include directories
inc = include_directories('.')

# Source files for core library
core_sources = files(
  # Core data module (NEW)
  'core/map_data.cpp',
  
  # Binary loader (NEW)
  'binary_loader/binary_database.cpp',
  'external_api_impl.cpp',
  
  # Coordinate conversions - dependencies fixed in Task 0.1
  'Coordinates_Converstions/convert_coords.cpp',
  'Coordinates_Converstions/get_zoom_level.cpp',
  'Coordinates_Converstions/map_bounds.cpp',
  
  # Intersections - dependencies fixed in Task 0.2
  'Intersections/intersection_setup.cpp',
  'Intersections/roadcolour.cpp',
  
  # POI (Point of Interest) - dependencies fixed in Task 0.3
  'POI/POI_helpers.cpp',
  'POI/POI_setup.cpp',
  
  # OSM Entity Helpers - dependencies fixed in Task 0.4
  'OSMEntity_Helpers/m2_way_helpers.cpp',
  'OSMEntity_Helpers/sort_features.cpp',
  'OSMEntity_Helpers/typed_osmid_helper.cpp',
  
  # Street segments - dependencies fixed in Task 0.5
  'sort_streetseg/colour_streets.cpp',
  
  # Spatial hash
  'spatial_hash/spatial_hash.cpp',
  
  # Pathfinding module (NEW)
  'pathfinding/astar.cpp',
  
  # Courier module (NEW) - temporarily disabled due to missing helper functions
  # 'courier/traveling_courier.cpp',
  
  # Stub implementations - REMOVED: Create proper implementations or use tool
  # See COMPILATION_FIXES_TODO.md Task C11 for stub API implementation requirements
  
  # Foursquare API - disabled due to missing external dependencies (curl, jsoncpp)
  # 'foursquareapi/create_Foursquare_POI_file.cpp',
  # 'foursquareapi/create_Foursquare_POI_objects.cpp',
)

# Note: This is a syntax-check library target
# It will fail at link time because we don't have StreetsDatabaseAPI library linked
# But it will show us compilation errors in our migrated code
gis_lib = shared_library('gisevo-core-lib',
  core_sources,
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep, tbb_dep],
  cpp_args: ['-Wno-unused-parameter', '-Wno-unused-variable'],
  install: true
)

gisevo_core_dep = declare_dependency(
  include_directories: inc,
  link_with: gis_lib
)

rtree_test = executable('rtree_test',
  'spatial_hash/rtree_test.cpp',
  include_directories: inc,
  cpp_args: ['-Wno-unused-parameter', '-Wno-unused-variable'],
  install: false
)

test('rtree_spatial_index', rtree_test)

benchmark_spatial_index = executable('benchmark_spatial_index',
  'spatial_hash/benchmark_spatial_index.cpp',
  include_directories: inc,
  cpp_args: ['-Wno-unused-parameter', '-Wno-unused-variable'],
  install: false
)

rtree_validation_test = executable('rtree_validation_test',
  'spatial_hash/rtree_validation_test.cpp',
  include_directories: inc,
  cpp_args: ['-Wno-unused-parameter', '-Wno-unused-variable'],
  install: false
)

test('rtree_validation', rtree_validation_test)
