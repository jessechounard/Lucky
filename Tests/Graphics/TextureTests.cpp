#include <doctest/doctest.h>

#include <Lucky/Texture.hpp>

using namespace Lucky;

TEST_CASE("IsCubeTextureType is true only for the cube targets") {
    CHECK_FALSE(IsCubeTextureType(TextureType::Default));
    CHECK_FALSE(IsCubeTextureType(TextureType::RenderTarget));
    CHECK_FALSE(IsCubeTextureType(TextureType::DepthTarget));
    CHECK(IsCubeTextureType(TextureType::CubeRenderTarget));
    CHECK(IsCubeTextureType(TextureType::CubeDepthTarget));
}

TEST_CASE("IsDepthTextureType is true only for the depth targets") {
    CHECK_FALSE(IsDepthTextureType(TextureType::Default));
    CHECK_FALSE(IsDepthTextureType(TextureType::RenderTarget));
    CHECK(IsDepthTextureType(TextureType::DepthTarget));
    CHECK_FALSE(IsDepthTextureType(TextureType::CubeRenderTarget));
    CHECK(IsDepthTextureType(TextureType::CubeDepthTarget));
}

TEST_CASE("BytesPerPixel returns the right size for each format") {
    CHECK(BytesPerPixel(TextureFormat::Normal) == 4);
    CHECK(BytesPerPixel(TextureFormat::HDR) == 8);
    CHECK(BytesPerPixel(TextureFormat::Depth) == 4);
}
