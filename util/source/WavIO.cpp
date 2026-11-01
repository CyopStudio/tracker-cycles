#include "PolyUtil/WavIO.h"

#include <sndfile.h>

namespace waveio
{

Result writeWavFile(const std::string& filename,
                    std::span<std::span<float>> buffers,
                    int sampleRate)
{
    auto frames = static_cast<sf_count_t>(buffers[0].size());
    auto channels = static_cast<int>(buffers.size());
    SF_INFO sfinfo{ .frames = frames,
                    .samplerate = sampleRate,
                    .channels = channels,
                    .format = SF_FORMAT_WAV | SF_FORMAT_FLOAT,
                    .sections = 1,
                    .seekable = 0 };
    auto* file = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);
    if (sf_error(file) != SF_ERR_NO_ERROR)
    {
        return { false, sf_strerror(file) };
    }

    std::vector<float> buffer(channels);
    for (int sample = 0; sample < frames; ++sample)
    {
        for (int i = 0; i < channels; ++i)
        {
            buffer[i] = buffers[i][sample];
        }

        auto written = sf_write_float(file, buffer.data(), channels);
        if (written != channels)
        {
            sf_close(file);
            return { false, "Failed to write all channels" };
        }
    }
    sf_close(file);
    return { true, "" };
}

Result writeWavFile(const std::string& filename, std::span<float> buffer, int sampleRate)
{
    SF_INFO sfinfo{ .frames = static_cast<sf_count_t>(buffer.size()),
                    .samplerate = sampleRate,
                    .channels = 1,
                    .format = SF_FORMAT_WAV | SF_FORMAT_FLOAT,
                    .sections = 1,
                    .seekable = 0 };
    auto* file = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);
    if (sf_error(file) != SF_ERR_NO_ERROR)
    {
        return { false, sf_strerror(file) };
    }

    auto written = sf_write_float(file, buffer.data(), std::ssize(buffer));
    if (written != std::ssize(buffer))
    {
        sf_close(file);
        return { false, "Failed to write the buffer" };
    }
    sf_close(file);
    return { true, "" };
}

Result readWavFile(const std::string& filename, std::span<float> buffer, int sampleRate)
{
    SF_INFO sfinfo{ .frames = static_cast<sf_count_t>(buffer.size()),
                    .samplerate = sampleRate,
                    .channels = 1,
                    .format = SF_FORMAT_WAV | SF_FORMAT_FLOAT,
                    .sections = 1,
                    .seekable = 0 };
    auto* file = sf_open(filename.c_str(), SFM_READ, &sfinfo);
    if (sf_error(file) != SF_ERR_NO_ERROR)
    {
        return { false, sf_strerror(file) };
    }

    sf_read_float(file, buffer.data(), std::ssize(buffer));
    sf_close(file);
    return { true, "" };
}

} // namespace waveio