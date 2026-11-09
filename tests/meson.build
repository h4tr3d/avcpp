#get catch2
catch2 = dependency('catch2-with-main', required: true)

deps = [avcpp_dep, catch2]

# Shared helpers for the generated test patterns
test_support = static_library(
    'test_support',
    'Patterns.cpp',
    dependencies: deps,
    include_directories: include_directories('.')
)
deps += declare_dependency(
    link_with: test_support,
    include_directories: include_directories('.')
)

tests = [
    'AvDeleter',
    'Buffer',
    'Codec',
    'CodecContext',
    'CodecContextSendReceive',
    'Common',
    'Format',
    'FormatCustomIO_test',
    'Frame',
    'Packet',
    'PixelSampleFormat',
    'Rational',
    'Timestamp'
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
