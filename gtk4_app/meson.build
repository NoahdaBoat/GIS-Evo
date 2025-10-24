gtk_dep = dependency('gtk4', required: true)
cairo_dep = dependency('cairo', required: true)
threads_dep = dependency('threads', required: true)
tbb_dep = dependency('tbb', required: false)

inc = include_directories('include', '../src')

sources = [
  'src/main.cpp',
  'src/map_view.cpp',
  'src/map_selector.cpp',
]

executable('gis-evo-gtk4',
  sources,
  include_directories: inc,
  dependencies: [gtk_dep, cairo_dep, threads_dep, tbb_dep, gisevo_core_dep],
  cpp_args: ['-Wno-unused-parameter', '-Wno-unused-variable'],
  install: true)
