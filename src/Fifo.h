#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace awchain
{
// Single producer, single consumer, lock free. Safe to pop on the audio thread.
template <typename T, int Capacity> class SpscQueue
{
  public:
    bool push(const T &item)
    {
        const auto scope = fifo.write(1);
        if (scope.blockSize1 < 1)
            return false;
        buffer[(size_t)scope.startIndex1] = item;
        return true;
    }

    bool pop(T &item)
    {
        const auto scope = fifo.read(1);
        if (scope.blockSize1 < 1)
            return false;
        item = buffer[(size_t)scope.startIndex1];
        return true;
    }

  private:
    juce::AbstractFifo fifo{Capacity};
    std::array<T, (size_t)Capacity> buffer{};
};
} // namespace awchain
