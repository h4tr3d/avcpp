project(
    'avcpp',
    'cpp',
    meson_version: '>= 0.50.0',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++17'
    ],
    version: '2.0.99',
)

#getting all of the dependencies
cc = meson.get_compiler('cpp')

avcpp_deps = [
    dependency('threads', required : true),
    dependency('libavcodec', required: true, fallback : ['ffmpeg', 'declared_dep']),
    dependency('libavformat', required: true, fallback : ['ffmpeg', 'declared_dep']),
    dependency('libavutil', required: true, fallback : ['ffmpeg', 'declared_dep']),
    dependency('libavdevice', required: true, fallback : ['ffmpeg', 'declared_dep']),
    dependency('libavfilter', required: true, fallback : ['ffmpeg', 'declared_dep']),
    dependency('libswscale', required: true, fallback : ['ffmpeg', 'declared_dep']),
    dependency('libswresample', required: true, fallback : ['ffmpeg', 'declared_dep']),
    cc.find_library('dl', required : false),
]

#setting the include dir to the current folder (src)
avcpp_incdir = include_directories('.','filters')

#listing all the source files
avcpp_sources = [
    'audioresampler.cpp',
    'averror.cpp',
    'avtime.cpp',
    'avutils.cpp',
    'codeccontext.cpp',
    'codec.cpp',
    'dictionary.cpp',
    'formatcontext.cpp',
    'format.cpp',
    'frame.cpp',
    'packet.cpp',
    'pixelformat.cpp',
    'rational.cpp',
    'rect.cpp',
    'sampleformat.cpp',
    'stream.cpp',
    'timestamp.cpp',
    'videorescaler.cpp',

    'filters/buffersink.cpp',
    'filters/buffersrc.cpp',
    'filters/filtercontext.cpp',
    'filters/filter.cpp',
    'filters/filtergraph.cpp',
    'filters/filterpad.cpp',

]

#defining how to build the library
avcpp = library(
    'avcpp', 
    avcpp_sources, 
    version : meson.project_version(), 
    soversion : '0', 
    include_directories : avcpp_incdir,
    dependencies: avcpp_deps
)

#export the dependency information
avcpp_dep = declare_dependency(
    include_directories : avcpp_incdir,
    link_with : avcpp,
    dependencies : avcpp_deps,
    version: meson.project_version()
)

#generate pkg-config
pkg = import('pkgconfig')
pkg.generate(avcpp)