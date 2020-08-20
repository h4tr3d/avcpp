project(
    'avcpp_tests',
    'cpp',
    version: '0.0.1',
    meson_version: '>= 0.50.0',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++17'
    ]
)

#get catch2
catch2 = dependency('catch2', required: false)
if not catch2.found()
    catch2_proj = subproject('catch2')
    catch2 = catch2_proj.get_variable('catch2_dep')
endif

#create main_test as library and export the dependency
main_test = static_library(
    'main_test', 
    'test-main.cpp', 
    dependencies: catch2
)

main_test_dep = declare_dependency(
    link_with : main_test,
    dependencies : catch2,
    version: meson.project_version()
)

#getting the dependencies for libavcpp
libavcpp = subproject('avcpp')
deps = [libavcpp.get_variable('avcpp_dep')]

#make both dependencies into one array
deps += main_test_dep

tests = [
    'Frame',
    'AvDeleter',
    'Packet',
]

#create all the tests
foreach test_obj : tests
    exe = executable(
        test_obj, 
        test_obj + '.cpp',
        dependencies: deps
    )
    test(test_obj + ' Test', exe)
endforeach
