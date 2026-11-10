#pragma once

#include <avcpp/frame.h>

namespace avcpp_test {

/**
 * Fills a video frame with a deterministic, high-contrast pattern that depends
 * on @p index. The pattern differs between consecutive calls, so an encoder
 * cannot collapse the sequence into identical output packets.
 *
 * The frame must carry an allocated pixel buffer (for example YUV420P).
 */
void fillVideoFrame(av::VideoFrame &frame, int index);

/**
 * Fills an audio buffer with a sine wave of @p frequency Hz.
 *
 * @p phase is advanced by the number of rendered samples, so consecutive
 * buffers produced with the same phase value stay continuous.
 */
void fillAudioSine(av::AudioSamples &samples, double frequency, double &phase);

} // namespace avcpp_test
