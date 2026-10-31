#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace askp {
// Independent SC220 telemetry tap. DSP callback is the single producer;
// display timer is the single consumer. Visual overflow drops samples.
class ScopeTap final
{
public:
    static constexpr int kSize = 8192;
    void setEnabled (bool value) noexcept { enabled.store (value, std::memory_order_release); }
    void push (const float* left, const float* right, int samples) noexcept
    {
        if (! enabled.load (std::memory_order_acquire) || left == nullptr || samples <= 0)
            return;
        if (right == nullptr) right = left;
        const int head = writePos;
        const int tail = publishedRead.load (std::memory_order_acquire);
        const int freeSpace = (tail - head - 1) & mask;
        const int count = juce::jmin (samples, freeSpace);
        const int first = juce::jmin (count, capacity - head);
        if (first > 0)
        {
            std::memcpy (leftSamples.data() + head, left, static_cast<size_t> (first) * sizeof (float));
            std::memcpy (rightSamples.data() + head, right, static_cast<size_t> (first) * sizeof (float));
        }
        if (count > first)
        {
            std::memcpy (leftSamples.data(), left + first, static_cast<size_t> (count-first) * sizeof (float));
            std::memcpy (rightSamples.data(), right + first, static_cast<size_t> (count-first) * sizeof (float));
        }
        writePos = (head + count) & mask;
        publishedWrite.store (writePos, std::memory_order_release);
    }
    int consumeLatest (float* left, float* right, int maximum) noexcept
    {
        if (left == nullptr || right == nullptr || maximum <= 0)
            return 0;
        const int head = publishedWrite.load (std::memory_order_acquire);
        int available = (head - readPos) & mask;
        maximum = juce::jlimit (0, kSize, maximum);
        const int take = juce::jmin (available, maximum);
        const int start = (head - take) & mask;
        const int first = juce::jmin (take, capacity - start);
        if (first > 0)
        {
            std::memcpy (left, leftSamples.data() + start, static_cast<size_t> (first)*sizeof (float));
            std::memcpy (right, rightSamples.data() + start, static_cast<size_t> (first)*sizeof (float));
        }
        if (take > first)
        {
            std::memcpy (left + first, leftSamples.data(), static_cast<size_t> (take-first)*sizeof (float));
            std::memcpy (right + first, rightSamples.data(), static_cast<size_t> (take-first)*sizeof (float));
        }
        readPos = head;
        publishedRead.store (head, std::memory_order_release);
        return take;
    }
private:
    static constexpr int capacity = 16384;
    static constexpr int mask = capacity-1;
    std::array<float,capacity> leftSamples{},rightSamples{};
    int writePos=0,readPos=0;
    std::atomic<int> publishedRead{0},publishedWrite{0};
    std::atomic<bool> enabled{false};
};
} // namespace askp
