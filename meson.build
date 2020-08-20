project(
    'AvCpp', 
    'c', 
    'cpp',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++17'
    ]
)

#add_project_arguments('-std=gnu++17', language : 'cpp')
cc = meson.get_compiler('cpp')

deps = [
    dependency('threads', required : true),
    dependency('ffmpeg', required : true, method : 'cmake', components : ['AVCODEC AVFORMAT AVUTIL AVDEVICE AVFILTER SWSCALE SWRESAMPLE']),
    cc.find_library('dl', required : false),
]

incdir = include_directories('src')

sources = [
    'src/*.cpp', 
]

AvCpp = library(
    'AvCpp', 
    sources, 
    version : '2.0.99', 
    soversion : '0', 
    include_directories : incdir,
    dependencies: deps
)

AvCpp_dep = declare_dependency(
    include_directories : incdir,
    link_with : AvCpp,
    dependencies : deps,
    version: '2.0.99'
