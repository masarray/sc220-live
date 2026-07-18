#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>

namespace askp::live {

// Current ArSonKuPik VST visual-tap architecture, adapted for the Live shell.
// It is a single-producer/single-consumer, visual-only queue. The audio callback
// never blocks: stale/new visual samples may be discarded, but audio is never
// delayed by a slow or hidden UI.
// The published indices are deliberately separated onto cache-line boundaries.
// MSVC C4324 reports that intentional padding, so suppress it only for this type.
#if defined(_MSC_VER)
 #pragma warning(push)
 #pragma warning(disable: 4324)
#endif
class VisualScopeTap final
{
public:
    static constexpr int maximumVisualTail = 4096;

    void setEnabled (bool shouldEnable) noexcept
    {
        enabled.store (shouldEnable, std::memory_order_release);
        if (! shouldEnable)
            clearRequested.store (true, std::memory_order_release);
    }

    bool isEnabled() const noexcept
    {
        return enabled.load (std::memory_order_acquire);
    }

    void push (const float* left, const float* right, int samples) noexcept
    {
        if (! enabled.load (std::memory_order_acquire) || left == nullptr || samples <= 0)
            return;

        const float* sourceRight = right != nullptr ? right : left;
        int write = producerWrite;
        const int read = readPublished.load (std::memory_order_acquire);
        const int writable = (read - write - 1) & (queueSize - 1);
        const int count = minimum (samples, writable);
        if (count <= 0)
        {
            droppedBlocks.fetch_add (1, std::memory_order_relaxed);
            return;
        }
        if (count < samples)
            droppedBlocks.fetch_add (1, std::memory_order_relaxed);

        const int first = minimum (count, queueSize - write);
        std::memcpy (leftQueue.data() + write, left,
                     static_cast<size_t> (first) * sizeof (float));
        std::memcpy (rightQueue.data() + write, sourceRight,
                     static_cast<size_t> (first) * sizeof (float));
        const int second = count - first;
        if (second > 0)
        {
            std::memcpy (leftQueue.data(), left + first,
                         static_cast<size_t> (second) * sizeof (float));
            std::memcpy (rightQueue.data(), sourceRight + first,
                         static_cast<size_t> (second) * sizeof (float));
        }

        write = (write + count) & (queueSize - 1);
        producerWrite = write;
        writePublished.store (write, std::memory_order_release);
    }

    // Consumes each captured sample once. When more data is waiting than the
    // analyser needs, the newest fixed-size tail is retained, matching the VST.
    int popLatest (float* left, float* right, int wanted) noexcept
    {
        wanted = limit (0, maximumVisualTail, wanted);
        if (left == nullptr || right == nullptr || wanted <= 0)
            return 0;

        if (clearRequested.exchange (false, std::memory_order_acq_rel))
        {
            consumerRead = writePublished.load (std::memory_order_acquire);
            readPublished.store (consumerRead, std::memory_order_release);
            return 0;
        }

        int read = consumerRead;
        const int published = writePublished.load (std::memory_order_acquire);
        int available = (published - read) & (queueSize - 1);
        if (available > wanted)
        {
            read = (published - wanted) & (queueSize - 1);
            available = wanted;
        }

        const int first = minimum (available, queueSize - read);
        if (first > 0)
        {
            std::memcpy (left, leftQueue.data() + read,
                         static_cast<size_t> (first) * sizeof (float));
            std::memcpy (right, rightQueue.data() + read,
                         static_cast<size_t> (first) * sizeof (float));
        }
        const int second = available - first;
        if (second > 0)
        {
            std::memcpy (left + first, leftQueue.data(),
                         static_cast<size_t> (second) * sizeof (float));
            std::memcpy (right + first, rightQueue.data(),
                         static_cast<size_t> (second) * sizeof (float));
        }

        consumerRead = published;
        readPublished.store (published, std::memory_order_release);
        return available;
    }

    unsigned int getDroppedBlocks() const noexcept
    {
        return droppedBlocks.load (std::memory_order_relaxed);
    }

private:
    template <typename T>
    static constexpr T minimum (T a, T b) noexcept { return a < b ? a : b; }

    template <typename T>
    static constexpr T limit (T low, T high, T value) noexcept
    {
        return value < low ? low : (value > high ? high : value);
    }

    static constexpr int queueSize = 16384;
    static_assert ((queueSize & (queueSize - 1)) == 0,
                   "visual queue size must be a power of two");

    alignas (64) std::array<float, queueSize> leftQueue {};
    alignas (64) std::array<float, queueSize> rightQueue {};
    int producerWrite = 0; // audio thread only
    int consumerRead = 0;  // message thread only
    alignas (64) std::atomic<int> writePublished { 0 };
    alignas (64) std::atomic<int> readPublished { 0 };
    std::atomic<unsigned int> droppedBlocks { 0 };
    std::atomic<bool> enabled { false };
    std::atomic<bool> clearRequested { true };
};
#if defined(_MSC_VER)
 #pragma warning(pop)
#endif

} // namespace askp::live
