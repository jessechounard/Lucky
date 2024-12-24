#pragma once

#include <cassert>
#include <stdint.h>
#include <type_traits>

#include <SquirrelNoise5.hpp>

namespace Lucky
{
    uint32_t GenerateRandom(int32_t position, uint32_t seed)
    {
        return SquirrelNoise5(position, seed);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, T>::type GenerateRandom(
        int32_t position, uint32_t seed, T inclusiveLowerBound, T inclusiveUpperBound)
    {
        assert(inclusiveUpperBound > inclusiveLowerBound);

        uint32_t rangeSize = inclusiveUpperBound - inclusiveLowerBound;
        uint32_t random = GenerateRandom(position, seed) % (rangeSize + 1);
        return (T)(inclusiveLowerBound + random);
    }

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, T>::type GenerateRandom(
        int32_t position, uint32_t seed, T inclusiveLowerBound, T inclusiveUpperBound)
    {
        assert(inclusiveUpperBound > inclusiveLowerBound);

        T t = (T)((double)GenerateRandom(position, seed) * (1.0 / (double)0xffffffff));
        return inclusiveLowerBound + t * (inclusiveUpperBound - inclusiveLowerBound);
    }
} // namespace Lucky
