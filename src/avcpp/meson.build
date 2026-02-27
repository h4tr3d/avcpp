if meson.project_name() == ''
    project(
        'avcpp',
        'cpp',
        meson_version: '>= 0.54.1',
        default_options : [
            'c_std=c11', 
            'cpp_std=c++20'
        ],
        version: files('VERSION.txt'),
    )
endif

# TBD
add_project_arguments('-D__STDC_CONSTANT_MACROS', language : ['c', 'cpp'])

#getting all of the dependencies
cc = meson.get_compiler('cpp')

#system
avcpp_deps = [
    dependency('threads', required : true),
    cc.find_library('dl', required : false),
]

#ffmpeg devpendecies
av_libs = [
    ['avcodec', '54.0.0'],
    ['avformat',  '54.0.0'],
    ['avutil', '51.0.0'],
    ['avdevice', '54.0.0'],
    ['avfilter', '3.0.0'],
    ['swscale', '2.0.0'],
    ['swresample', '0.0.0'],
    ['postproc', '52.0.0'],
]

conf_data = configuration_data()
cpp_std = get_option('cpp_std')
if cpp_std in ['c++17', 'gnu++17', 'vc++17']
    conf_data.set('AVCPP_CXX_STANDARD', '17')
elif cpp_std in ['c++20', 'gnu++20', 'vc++20']
    conf_data.set('AVCPP_CXX_STANDARD', '20')
elif cpp_std in ['c++23', 'gnu++23', 'vc++23']
    conf_data.set('AVCPP_CXX_STANDARD', '23')
elif cpp_std in ['c++26', 'gnu++26', 'vc++26']
    conf_data.set('AVCPP_CXX_STANDARD', '26')
else
    warning('C++ standard setting ', cpp_std, ' not recognized')
endif

foreach lib : av_libs
    lib_dep =  dependency(
            'lib@0@'.format(lib[0]),
            required: true,
            fallback : ['ffmpeg', 'lib@0@_dep'.format(lib[0])],
            version : '>=@0@'.format(lib[1]))
    avcpp_deps += [ lib_dep ]
    lib_version = lib_dep.version().split('.')
    conf_data.set(
            'AVCPP_' + lib[0].to_upper() + '_VERSION_MAJOR',
            lib_version[0])
    conf_data.set(
            'AVCPP_' + lib[0].to_upper() + '_VERSION_MINOR',
            lib_version[1])
    conf_data.set(
            'AVCPP_' + lib[0].to_upper() + '_VERSION_PATCH',
            lib_version[2])
    conf_data.set(
            'AVCPP_' + lib[0].to_upper() + '_VERSION_TWEAK',
            '0')
endforeach

configure_file(input : 'avconfig.h.in',
               output : 'avconfig.h',
               configuration : conf_data,
               format: 'cmake',
               install: true,
               install_dir: 'include/avcpp'
               )

#setting the include dir to the current folder (src)
avcpp_incdir = include_directories('.','filters','..')

#listing all the source files
avcpp_sources = [
    'audioresampler.cpp',
    'averror.cpp',
    'avtime.cpp',
    'avutils.cpp',
    'channellayout.cpp',
    'codeccontext.cpp',
    'codec.cpp',
    'codecparameters.cpp',
    'buffer.cpp',
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

avcpp_header = [
    'audioresampler.h',
    'averror.h',
    'av.h',
    'avlog.h',
    'avtime.h',
    'avutils.h',
    'channellayout.h',
    'codeccontext.h',
    'codec.h',
    'codecparameters.h',
    'dictionary.h',
    'ffmpeg.h',
    'formatcontext.h',
    'format.h',
    'frame.h',
    'linkedlistutils.h',
    'packet.h',
    'pixelformat.h',
    'rational.h',
    'rect.h',
    'sampleformat.h',
    'stream.h',
    'timestamp.h',
    'videorescaler.h',
]

avcpp_filter_header = [
    'filters/buffersink.h',
    'filters/buffersrc.h',
    'filters/filtercontext.h',
    'filters/filtergraph.h',
    'filters/filter.h',
    'filters/filterpad.h',
]

#defining how to build the library
avcpp = library(
    'avcpp', 
    avcpp_sources, 
    version : meson.project_version(), 
    soversion : '0', 
    include_directories : avcpp_incdir,
    dependencies: avcpp_deps, 
    install : true
)

install_headers(avcpp_header, subdir : 'avcpp')
install_headers(avcpp_filter_header, subdir : 'avcpp/filters')


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
