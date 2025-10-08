converter_inc = include_directories('include', '../../third_party/libosmium/include')

cpp = meson.get_compiler('cpp')

if cpp.has_header('osmium/io/reader.hpp', required: true)
  libosmium_dep = declare_dependency()
else
  error('libosmium headers not found; install libosmium2-dev or set include path')
endif

if cpp.has_header('protozero/pbf_reader.hpp', required: false)
  protozero_dep = declare_dependency()
else
  protozero_dep = dependency('protozero', required: false)
endif
zlib_dep = dependency('zlib', required: true)
threads_dep = dependency('threads')

deps = [libosmium_dep]
if protozero_dep.found()
  deps += protozero_dep
endif
deps += [threads_dep, zlib_dep]

executable('osm_converter',
  ['src/main.cpp', 'src/converter.cpp'],
  dependencies: deps,
  include_directories: converter_inc,
  cpp_args: ['-DOSMIUM_WITH_PBF_INPUT', '-DOSMIUM_WITH_PROTOZERO'],
  install: true)
