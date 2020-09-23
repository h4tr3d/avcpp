project(
    'avcpp',
    'cpp',
    meson_version: '>= 0.54.0',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++17',
        'build_tests=@0@'.format(not meson.is_subproject()),
        'build_samples=@0@'.format(not meson.is_subproject()),
    ],
    version: '2.0.99',
)

subdir('src')
subdir('example/api2-samples')
subdir('tests')

meson.override_dependency('avcpp', avcpp_dep)

summary({
    'Build samples' : get_option('build_samples'),
    'Build tests' : get_option('build_tests')
    }, section: 'Extra', bool_yn: true,
)
