project(
    'AvCpp', 
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

subproject('libavcpp')
subproject('AvCppSamples')


