#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <spdlog/spdlog.h>
#include <stb_image.h>

#include <Lucky/Graphics/Texture.hpp>

#include "IncludeOpenGL.h"

namespace Lucky
{
    Texture::Texture(const std::string &filename, TextureFilter textureFilter, TextureFormat textureFormat)
    {
        int imageWidth, imageHeight, imageChannels;
        uint8_t *imagePixels = stbi_load(filename.c_str(), &imageWidth, &imageHeight, &imageChannels, 4);
        if (imagePixels == nullptr)
        {
            spdlog::error("Failed to load image file: {}", filename);
            throw;
        }

        Initialize(TextureType::Default, imageWidth, imageHeight, imagePixels, imageWidth * imageHeight * 4,
            textureFilter, textureFormat);
        stbi_image_free(imagePixels);
    }

    Texture::Texture(uint8_t *memory, uint32_t memoryLength, TextureFilter textureFilter, TextureFormat textureFormat)
    {
        assert(memory != nullptr);

        int imageWidth, imageHeight, imageChannels;
        uint8_t *imagePixels =
            stbi_load_from_memory(memory, memoryLength, &imageWidth, &imageHeight, &imageChannels, 4);
        if (imagePixels == nullptr)
        {
            spdlog::error("Failed to load image file from memory");
            throw;
        }

        Initialize(TextureType::Default, imageWidth, imageHeight, imagePixels, imageWidth * imageHeight * 4,
            textureFilter, textureFormat);
        stbi_image_free(imagePixels);
    }

    Texture::Texture(TextureType textureType, uint32_t width, uint32_t height, uint8_t *pixelData, uint32_t dataLength,
        TextureFilter textureFilter, TextureFormat textureFormat)
    {
        assert(width > 0);
        assert(height > 0);
        if (pixelData != nullptr)
        {
            assert(dataLength >= width * height * 4);
        }

        Initialize(textureType, width, height, pixelData, dataLength, textureFilter, textureFormat);
    }

    void Texture::Initialize(TextureType textureType, uint32_t width, uint32_t height, uint8_t *pixelData,
        uint32_t dataLength, TextureFilter textureFilter, TextureFormat textureFormat)
    {
        this->width = width;
        this->height = height;
        this->textureType = textureType;

        glGenTextures(1, &textureId);

        SetTextureFilter(textureFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLint internalFormat = textureFormat == TextureFormat::Normal ? GL_RGBA : GL_R11F_G11F_B10F;

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);

        if (textureType == TextureType::RenderTarget)
        {
            int currentFramebufferObject;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFramebufferObject);

            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                spdlog::error("Failed to create render target texture");
                throw;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, currentFramebufferObject);
        }
    }

    Texture::~Texture()
    {
        if (textureType == TextureType::RenderTarget)
        {
            glDeleteFramebuffers(1, &fbo);
        }

        glDeleteTextures(1, &textureId);
    }

    void Texture::SetTextureData(
        uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t *pixelData, uint32_t dataLength)
    {
        assert(pixelData != nullptr);
        assert(x + w <= width);
        assert(y + h <= height);
        assert(dataLength == w * h * 4);

        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
    }

    void Texture::SetTextureFilter(TextureFilter filter)
    {
        textureFilter = filter;

        glBindTexture(GL_TEXTURE_2D, textureId);

        switch (textureFilter)
        {
        case TextureFilter::Linear:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;

        case TextureFilter::Point:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;

        default:
            spdlog::error("Unsupported TextureFilter type.");
            throw;
        }
    }
} // namespace Lucky