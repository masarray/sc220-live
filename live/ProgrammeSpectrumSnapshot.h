#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace askp::live {

// One producer (the JUCE audio callback) and one consumer (the message thread).
// The producer accumulates exactly 512 post-limiter stereo samples, publishes a
// complete bank, and immediately continues in the other bank. The UI never sees
// a partially written frame and the audio thread never waits or allocates.
class ProgrammeSpectrumSnapshot final
{
public:
    static constexpr int frameSize = 512;

    void reset() noexcept
    {
        writeBank = 0;
        writeOffset = 0;
        consumerGeneration = 0;
        publishedBank.store (-1, std::memory_order_release);
        publishedGeneration.store (0, std::memory_order_release);
        for (auto& bank : banks)
        {
            bank.sequence.store (0, std::memory_order_release);
            bank.left.fill (0.0f);
            bank.right.fill (0.0f);
        }
    }

    void push (const float* left, const float* right, int samples) noexcept
    {
        if (left == nullptr || right == nullptr || samples <= 0)
            return;

        int sourceOffset = 0;
        while (sourceOffset < samples)
        {
            auto& bank = banks[static_cast<std::size_t> (writeBank)];
            if (writeOffset == 0)
                bank.sequence.fetch_add (1, std::memory_order_acq_rel); // odd: writing

            const int count = std::min (frameSize - writeOffset,
                                        samples - sourceOffset);
            std::memcpy (bank.left.data() + writeOffset,
                         left + sourceOffset,
                         static_cast<std::size_t> (count) * sizeof (float));
            std::memcpy (bank.right.data() + writeOffset,
                         right + sourceOffset,
                         static_cast<std::size_t> (count) * sizeof (float));
            writeOffset += count;
            sourceOffset += count;

            if (writeOffset == frameSize)
            {
                bank.sequence.fetch_add (1, std::memory_order_release); // even: complete
                publishedBank.store (writeBank, std::memory_order_release);
                publishedGeneration.fetch_add (1, std::memory_order_release);
                writeBank ^= 1;
                writeOffset = 0;
            }
        }
    }

    int consumeLatest (float* leftDestination, float* rightDestination,
                       int maximumSamples) noexcept
    {
        if (leftDestination == nullptr || rightDestination == nullptr
            || maximumSamples < frameSize)
            return 0;

        const auto generation = publishedGeneration.load (std::memory_order_acquire);
        if (generation == 0 || generation == consumerGeneration)
            return 0;

        const int bankIndex = publishedBank.load (std::memory_order_acquire);
        if (bankIndex < 0 || bankIndex > 1)
            return 0;

        const auto& bank = banks[static_cast<std::size_t> (bankIndex)];
        const auto sequenceBefore = bank.sequence.load (std::memory_order_acquire);
        if ((sequenceBefore & 1U) != 0U)
            return 0;

        std::memcpy (leftDestination, bank.left.data(),
                     static_cast<std::size_t> (frameSize) * sizeof (float));
        std::memcpy (rightDestination, bank.right.data(),
                     static_cast<std::size_t> (frameSize) * sizeof (float));

        const auto sequenceAfter = bank.sequence.load (std::memory_order_acquire);
        if (sequenceBefore != sequenceAfter || (sequenceAfter & 1U) != 0U)
            return 0;

        consumerGeneration = generation;
        return frameSize;
    }

    std::uint64_t generation() const noexcept
    {
        return publishedGeneration.load (std::memory_order_acquire);
    }

private:
    struct Bank
    {
        std::array<float, frameSize> left {};
        std::array<float, frameSize> right {};
        std::atomic<std::uint32_t> sequence { 0 };
    };

    std::array<Bank, 2> banks {};
    int writeBank = 0;                 // audio callback only
    int writeOffset = 0;               // audio callback only
    std::uint64_t consumerGeneration = 0; // message thread only
    std::atomic<int> publishedBank { -1 };
    std::atomic<std::uint64_t> publishedGeneration { 0 };
};

// C++17 inline variable: exactly one process-wide instance shared by the
// generated engine and generated UI translation units.
inline ProgrammeSpectrumSnapshot programmeSpectrumSnapshot;

} // namespace askp::live
