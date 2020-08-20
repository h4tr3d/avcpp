project(
    'AvCppSamples',
    'cpp',
    version: '2.0.99'
)

libavcpp = subproject('libavcpp')

avcpp_dep = libavcpp.get_variable('libavcpp_dep')

samples = [
    'api2-decode',
    'api2-decode-encode-video',
    'api2-scale-video',
    'api2-decode-audio',
    'api2-decode-rasample-audio',
    'api2-decode-encode-audio',
    'api2-dict-basic',
    'api2-timestamp',
    'api2-demux-seek',
    'api2-remux',
]

foreach sample : samples
    executable(
        sample, 
        sample + '.cpp',
        dependencies: avcpp_dep
    )
endforeach


