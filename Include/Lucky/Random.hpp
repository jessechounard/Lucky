#pragma once

#include <SDL3/SDL_assert.h>
#include <stdint.h>
#include <type_traits>

#include <SquirrelNoise5.hpp>

namespace Lucky {

/**
 * Position-based deterministic random numbers, backed by SquirrelNoise5.
 *
 * Unlike a streaming RNG (`rng.next()`), these functions are pure hashes
 * of `(position, seed)` -- the same inputs always produce the same output.
 * This is the right model for procedural generation where you want to
 * regenerate the same world from a seed, sample a noise field across
 * frames without storing per-cell state, or get reproducible results for
 * debugging.
 *
 * If you need a streaming RNG (independent draws without thinking about
 * positions), wrap a counter:
 *
 *     int32_t cursor = 0;
 *     uint32_t a = GenerateRandom(cursor++, seed);
 *     uint32_t b = GenerateRandom(cursor++, seed);
 *
 * # Modulo bias
 *
 * The integer-range overload uses `random % (rangeSize + 1)`, which
 * introduces a small bias when `rangeSize + 1` does not divide evenly
 * into 2^32. For typical game ranges (a few thousand or fewer) the bias
 * is sub-percent. If you need uniform sampling for cryptography or large
 * ranges, this is not the right tool.
 *
 * \returns a uniform-ish 32-bit value derived from `(position, seed)`.
 *
 * \param position the input coordinate; sampled deterministically.
 * \param seed an additional input; different seeds yield different output
 *             sequences for the same `position`.
 */
inline uint32_t GenerateRandom(int32_t position, uint32_t seed) {
    return SquirrelNoise5(position, seed);
}

/**
 * Returns a deterministic random integer in [inclusiveLowerBound,
 * inclusiveUpperBound].
 *
 * Asserts in debug if `inclusiveUpperBound <= inclusiveLowerBound`. See
 * the file-level "Modulo bias" note for sampling-uniformity caveats.
 *
 * \param position deterministic input coordinate.
 * \param seed additional input; different seeds give different sequences.
 * \param inclusiveLowerBound smallest possible result.
 * \param inclusiveUpperBound largest possible result; must be strictly
 *                            greater than the lower bound.
 * \returns a value in `[inclusiveLowerBound, inclusiveUpperBound]`.
 */
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type GenerateRandom(
    int32_t position, uint32_t seed, T inclusiveLowerBound, T inclusiveUpperBound) {
    SDL_assert(inclusiveUpperBound > inclusiveLowerBound);

    uint32_t rangeSize = inclusiveUpperBound - inclusiveLowerBound;
    uint32_t random = GenerateRandom(position, seed) % (rangeSize + 1);
    return (T)(inclusiveLowerBound + random);
}

/**
 * Returns a deterministic random float in [inclusiveLowerBound,
 * inclusiveUpperBound].
 *
 * Asserts in debug if `inclusiveUpperBound <= inclusiveLowerBound`. The
 * upper bound is reachable but rare (requires the underlying hash to
 * return 0xFFFFFFFF exactly).
 *
 * \param position deterministic input coordinate.
 * \param seed additional input; different seeds give different sequences.
 * \param inclusiveLowerBound smallest possible result.
 * \param inclusiveUpperBound largest possible result; must be strictly
 *                            greater than the lower bound.
 * \returns a value in `[inclusiveLowerBound, inclusiveUpperBound]`.
 */
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type GenerateRandom(
    int32_t position, uint32_t seed, T inclusiveLowerBound, T inclusiveUpperBound) {
    SDL_assert(inclusiveUpperBound > inclusiveLowerBound);

    T t = (T)((double)GenerateRandom(position, seed) * (1.0 / (double)0xffffffff));
    return inclusiveLowerBound + t * (inclusiveUpperBound - inclusiveLowerBound);
}

} // namespace Lucky
