project('gis-evo-clean', 'cpp',
  version: '1.0.0',
  default_options: ['cpp_std=c++17'])

# Clean Architecture Build Configuration
# This builds a new, modern GIS application without legacy dependencies

gtk_dep = dependency('gtk4', required: true)
cairo_dep = dependency('cairo', required: true)
threads_dep = dependency('threads', required: true)

# Include directories
inc = include_directories('.', '..', '../binary_loader', '../spatial_hash')

# Core clean architecture sources
core_sources = files(
  'map_data.cpp',
  'renderer.cpp', 
  'ui.cpp',
  'main.cpp',
  # Include binary loader for data loading
  '../binary_loader/binary_database.cpp',
  '../binary_loader/cache_manager.cpp',
  '../external_api_impl.cpp',
  '../spatial_hash/spatial_hash.cpp'
)

# Create the clean architecture executable
executable('gis-evo-clean',
  core_sources,
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-std=c++17', '-Wall', '-Wextra'],
  install: true)

# Create integration test executable
executable('test-integration',
  files('test_integration.cpp', 'map_data.cpp', '../binary_loader/binary_database.cpp', '../binary_loader/cache_manager.cpp', '../external_api_impl.cpp', '../spatial_hash/spatial_hash.cpp'),
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-std=c++17', '-Wall', '-Wextra'],
  install: false)

executable('test-performance-regression',
  files('test_performance_regression.cpp', 'map_data.cpp', '../binary_loader/binary_database.cpp', '../binary_loader/cache_manager.cpp', '../external_api_impl.cpp', '../spatial_hash/spatial_hash.cpp'),
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-std=c++17', '-Wall', '-Wextra'],
  install: false)

# Create simple test executable
executable('test-simple',
  files('test_simple.cpp', 'map_data.cpp', '../binary_loader/binary_database.cpp', '../binary_loader/cache_manager.cpp', '../external_api_impl.cpp', '../spatial_hash/spatial_hash.cpp'),
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-std=c++17', '-Wall', '-Wextra'],
  install: false)

# Create feature test executable
executable('test-features',
  files('test_features.cpp', '../binary_loader/binary_database.cpp', '../binary_loader/cache_manager.cpp', '../external_api_impl.cpp'),
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-std=c++17', '-Wall', '-Wextra'],
  install: false)

# Create cache debug test executable
executable('test-cache-debug',
  files('test_cache_debug.cpp', '../binary_loader/binary_database.cpp', '../binary_loader/cache_manager.cpp', '../external_api_impl.cpp', '../spatial_hash/spatial_hash.cpp'),
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-std=c++17', '-Wall', '-Wextra'],
  install: false)

# Optional: Create a library for the core components
core_lib = library('gisevo-core-clean',
  files(
    'map_data.cpp',
    'renderer.cpp',
    'ui.cpp',
    '../binary_loader/binary_database.cpp',
    '../binary_loader/cache_manager.cpp',
    '../external_api_impl.cpp',
    '../spatial_hash/spatial_hash.cpp'
  ),
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep],
  cpp_args: ['-std=c++17', '-Wall', '-Wextra'],
  install: true)