#include "Patterns.hpp"

#include <cmath>
#include <cstdint>

namespace avcpp_test {

void fillVideoFrame(av::VideoFrame &frame, int index)
{
    if (!frame.isValid())
        return;

    // Fill every allocated byte, so the result does not depend on the stride
    // and the luma/chroma planes always contain a moving pattern.
    for (std::size_t plane = 0; plane < 3; ++plane) {
        auto *data = frame.data(plane);
        if (!data)
            continue;

        auto const size = frame.size(plane);
        if (size == 0)
            continue;

        auto const shift = static_cast<std::uint8_t>((index * 37 + plane * 11) & 0xFF);
        for (std::size_t i = 0; i < size; ++i) {
            auto const ramp = static_cast<std::uint8_t>((i * 5 + index * 17) & 0xFF);
            data[i] = static_cast<std::uint8_t>(ramp ^ shift);
        }
    }
}

void fillAudioSine(av::AudioSamples &samples, double frequency, double &phase)
{
    if (!samples.isValid())
        return;

    auto const sampleRate = samples.sampleRate();
    if (sampleRate <= 0)
        return;

    constexpr double Pi = 3.14159265358979323846;
    auto const step = 2.0 * Pi * frequency / sampleRate;

    // The generated fixtures use S16 samples. Iterate over every allocated
    // plane, which covers both packed and planar layouts.
    for (std::size_t plane = 0; plane < AV_NUM_DATA_POINTERS; ++plane) {
        auto *data = reinterpret_cast<std::int16_t *>(samples.data(plane));
        if (!data)
            continue;

        auto const count = samples.size(plane) / sizeof(std::int16_t);
        for (std::size_t i = 0; i < count; ++i) {
            data[i] = static_cast<std::int16_t>(std::sin(phase) * 0.5 * 32767.0);
            phase += step;
        }
    }

    // Keep the phase bounded to avoid precision loss over long sequences.
    phase = std::fmod(phase, 2.0 * Pi);
}

} // namespace avcpp_test
