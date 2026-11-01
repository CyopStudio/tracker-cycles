#pragma once

#include <cassert>
#include <span>
#include <string>
#include <vector>

namespace waveio
{

struct Result
{
    bool success;
    std::string errorMessage;
};

Result writeWavFile(const std::string& filename, std::span<std::span<float>> buffers, int sampleRate);
Result writeWavFile(const std::string& filename, std::span<float> buffer, int sampleRate);
Result readWavFile(const std::string& filename, std::span<float> buffer, int sampleRate);

} // namespace waveio