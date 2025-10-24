xml_to_pbf_converter_inc = include_directories('include', '../../third_party/libosmium/include')

cpp = meson.get_compiler('cpp')

if cpp.has_header('osmium/io/reader.hpp', required: false, include_directories: xml_to_pbf_converter_inc)
  libosmium_dep = declare_dependency()
else
  warning('libosmium headers not found; skipping xml_to_pbf_converter tool')
  subdir_done()
endif

if cpp.has_header('protozero/pbf_reader.hpp', required: false)
  protozero_dep = declare_dependency()
else
  protozero_dep = dependency('protozero', required: false)
endif
zlib_dep = dependency('zlib', required: true)
threads_dep = dependency('threads')
expat_dep = dependency('expat', required: true)
bzip2_dep = cpp.find_library('bz2', required: true)

deps = [libosmium_dep]
if protozero_dep.found()
  deps += protozero_dep
endif
deps += [threads_dep, zlib_dep, expat_dep, bzip2_dep]

executable('xml_to_pbf_converter',
  ['src/main.cpp', 'src/converter.cpp'],
  dependencies: deps,
  include_directories: xml_to_pbf_converter_inc,
  cpp_args: ['-DOSMIUM_WITH_XML_INPUT', '-DOSMIUM_WITH_PBF_OUTPUT', '-DOSMIUM_WITH_PROTOZERO'],
  install: true)
