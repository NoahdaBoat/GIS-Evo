# Build configuration for GIS-Evo core library
# This is a compilation test target to check migrated files for errors

gtk_dep = dependency('gtk4', required: true)
cairo_dep = dependency('cairo', required: true)
threads_dep = dependency('threads', required: true)

# Include directories
inc = include_directories('.')

# Source files for core library
core_sources = files(
  # Main API files
  'm1.cpp',
  'm2.cpp',
  'm3.cpp',
  'm4.cpp',
  
  # Helper files
  'ms1helpers.cpp',
  'ms2helpers.cpp',
  'ms3helpers.cpp',
  'ms4helpers.cpp',
  
  # Coordinate conversions
  'Coordinates_Converstions/convert_coords.cpp',
  'Coordinates_Converstions/get_zoom_level.cpp',
  'Coordinates_Converstions/map_bounds.cpp',
  
  # Intersections
  'Intersections/intersection_setup.cpp',
  'Intersections/roadcolour.cpp',
  
  # POI (Point of Interest)
  'POI/POI_helpers.cpp',
  'POI/POI_setup.cpp',
  
  # OSM Entity Helpers
  'OSMEntity_Helpers/m2_way_helpers.cpp',
  'OSMEntity_Helpers/sort_features.cpp',
  'OSMEntity_Helpers/typed_osmid_helper.cpp',
  
  # Street segments
  'sort_streetseg/colour_streets.cpp',
  
  # Spatial hash
  'spatial_hash/spatial_hash.cpp',
  
  # M3 Algorithm
  'm3_algo/astaralgo.cpp',
  
  # Foursquare API
  'foursquareapi/create_Foursquare_POI_file.cpp',
  'foursquareapi/create_Foursquare_POI_objects.cpp',
)

# Note: This is a syntax-check library target
# It will fail at link time because we don't have StreetsDatabaseAPI library linked
# But it will show us compilation errors in our migrated code
gis_lib = library('gisevo-core',
  core_sources,
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-Wno-unused-parameter', '-Wno-unused-variable'],
  install: false
)
