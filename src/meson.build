project(
    'libavcpp', 
    'c', 
    'cpp',
    meson_version: '>= 0.50.0',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++17'
    ],
    version: '2.0.99',
)

#add_project_arguments('-std=gnu++17', language : 'cpp')
cc = meson.get_compiler('cpp')

libavcpp_deps = [
    dependency('threads', required : true),
    #dependency('FFmpeg', required : true, method : 'cmake', components : ['AVCODEC AVFORMAT AVUTIL AVDEVICE AVFILTER SWSCALE SWRESAMPLE']),
    dependency('libavcodec', required: true),
    dependency('libavformat', required: true),
    dependency('libavutil', required: true),
    dependency('libavdevice', required: true),
    dependency('libavfilter', required: true),
    dependency('libswscale', required: true),
    dependency('libswresample', required: true),
    cc.find_library('dl', required : false),
]

libavcpp_incdir = include_directories('.')

libavcpp_sources = [
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
    #'libavcpp.pc.in',
    'packet.cpp',
    'pixelformat.cpp',
    'rational.cpp',
    'rect.cpp',
    'sampleformat.cpp',
    'stream.cpp',
    'timestamp.cpp',
    'videorescaler.cpp',
]

libavcpp = library(
    'libavcpp', 
    libavcpp_sources, 
    version : meson.project_version(), 
    soversion : '0', 
    include_directories : libavcpp_incdir,
    dependencies: libavcpp_deps
)

libavcpp_dep = declare_dependency(
    include_directories : libavcpp_incdir,
    link_with : libavcpp,
    dependencies : libavcpp_deps,
    version: meson.project_version()
)

