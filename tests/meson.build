#get catch2
catch2 = dependency('catch2', required: true, fallback:['catch2','catch2_dep'])

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

#make both dependencies into one array
deps = [avcpp_dep]
deps += main_test_dep

tests = [
    'Frame',
    'AvDeleter',
    'Packet',
    'Format',
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
