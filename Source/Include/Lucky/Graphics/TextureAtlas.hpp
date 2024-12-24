#pragma once

#include <map>
#include <memory>
#include <stdint.h>
#include <string>

#include <glm/glm.hpp>

#include <Lucky/Math/Rectangle.hpp>

namespace Lucky
{
    struct TextureRegion
    {
        Rectangle bounds;
        Rectangle input;

        glm::vec2 originTopLeft;
        glm::vec2 originBottomRight;
        glm::vec2 originCenter;
        glm::vec2 pivot;

        bool rotated;
    };

    struct TextureAtlas
    {
      public:
        TextureAtlas(const std::string &fileName);
        TextureAtlas(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName = "");

        bool Contains(const std::string &textureName) const;
        TextureRegion GetRegion(const std::string &textureName) const;

        const std::string &TexturePath() const
        {
            return texturePath;
        }

      private:
        void Initialize(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName);

        std::string texturePath;
        std::map<std::string, TextureRegion> dictionary;
    };
} // namespace Lucky
