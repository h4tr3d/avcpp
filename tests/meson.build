#get catch2
catch2 = dependency('catch2-with-main', required: true)

deps = [avcpp_dep, catch2]

tests = [
    'AvDeleter',
    'Buffer',
    'Codec',
    'Format',
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
