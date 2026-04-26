#include <cstdint>

#include <doctest/doctest.h>

#include <Lucky/IndexBuffer.hpp>

using namespace Lucky;

TEST_CASE("IndexElementSizeOf maps uint16_t to 16-bit indices") {
    CHECK(IndexElementSizeOf<uint16_t>() == SDL_GPU_INDEXELEMENTSIZE_16BIT);
}

TEST_CASE("IndexElementSizeOf maps uint32_t to 32-bit indices") {
    CHECK(IndexElementSizeOf<uint32_t>() == SDL_GPU_INDEXELEMENTSIZE_32BIT);
}
