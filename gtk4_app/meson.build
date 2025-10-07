gtk_dep = dependency('gtk4', required: true)

inc = include_directories('include')

sources = [
  'src/main.cpp',
  'src/map_view.cpp',
]

executable('gis-evo-gtk4',
  sources,
  include_directories: inc,
  dependencies: [gtk_dep],
  install: true)
