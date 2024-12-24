#include <fstream>
#include <stdexcept>

#include <rapidjson/document.h>
#include <spdlog/spdlog.h>

// #include <Lucky/Content/Content.hpp>
#include <Lucky/Graphics/TextureAtlas.hpp>
#include <Lucky/Math/MathHelpers.hpp>
#include <Lucky/Utility/FileSystem.hpp>

namespace Lucky
{
    TextureAtlas::TextureAtlas(const std::string &fileName)
    {
        std::ifstream stream(fileName, std::ios::in | std::ios::binary);
        if (!stream)
        {
            spdlog::error("Failed to load texture dictionary file: {}", fileName);
            throw;
        }

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (buffer.size() == 0)
        {
            spdlog::error("Failed to read texture dictionary: {}", fileName);
            throw;
        }

        Initialize(buffer.data(), buffer.size(), fileName);
    }

    TextureAtlas::TextureAtlas(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName)
    {
        Initialize(buffer, bufferLength, fileName);
    }

    void TextureAtlas::Initialize(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName)
    {
        rapidjson::Document jsonDocument;

        auto &x = jsonDocument.Parse((char *)buffer, (size_t)bufferLength);
        if (x.HasParseError())
        {
            spdlog::error("JSON parsing failed for file: {}", fileName);
            throw;
        }

        auto &frames = jsonDocument["frames"];

        for (auto &frame : frames.GetArray())
        {
            std::string name = frame["filename"].GetString();

            TextureRegion textureRegion;
            textureRegion.bounds.x = frame["frame"]["x"].GetInt();
            textureRegion.bounds.y = frame["frame"]["y"].GetInt();
            textureRegion.bounds.width = frame["frame"]["w"].GetInt();
            textureRegion.bounds.height = frame["frame"]["h"].GetInt();
            textureRegion.rotated = frame["rotated"].GetBool();

            float spriteSourceSizeX = frame["spriteSourceSize"]["x"].GetFloat();
            float spriteSourceSizeY = frame["spriteSourceSize"]["y"].GetFloat();
            float spriteSourceSizeW = frame["spriteSourceSize"]["w"].GetFloat();
            float spriteSourceSizeH = frame["spriteSourceSize"]["h"].GetFloat();

            float sourceSizeW = frame["sourceSize"]["w"].GetFloat();
            float sourceSizeH = frame["sourceSize"]["h"].GetFloat();

            textureRegion.originTopLeft.x = -spriteSourceSizeX / spriteSourceSizeW;
            textureRegion.originTopLeft.y = -spriteSourceSizeY / spriteSourceSizeH;

            textureRegion.originBottomRight.x =
                1 + (sourceSizeW - (spriteSourceSizeX + spriteSourceSizeW)) / spriteSourceSizeW;
            textureRegion.originBottomRight.y =
                1 + (sourceSizeH - (spriteSourceSizeY + spriteSourceSizeH)) / spriteSourceSizeH;

            textureRegion.originCenter = (textureRegion.originTopLeft + textureRegion.originBottomRight) / 2.0f;

            glm::vec2 pivot = {0.5f, 0.5f};
            if (frame.HasMember("pivot"))
            {
                pivot.x = frame["pivot"]["x"].GetFloat();
                pivot.y = frame["pivot"]["y"].GetFloat();
            }

            textureRegion.pivot.x =
                Lerp<float>(textureRegion.originTopLeft.x, textureRegion.originBottomRight.x, pivot.x);
            textureRegion.pivot.y =
                Lerp<float>(textureRegion.originTopLeft.y, textureRegion.originBottomRight.y, pivot.y);

            textureRegion.input.x = (int)spriteSourceSizeX;
            textureRegion.input.y = (int)spriteSourceSizeY;
            textureRegion.input.width = (int)sourceSizeW;
            textureRegion.input.height = (int)sourceSizeH;

            dictionary[name] = textureRegion;
        }

        auto &meta = jsonDocument["meta"];
        texturePath = CombinePaths(GetPathName(fileName), meta["image"].GetString());
    }

    bool TextureAtlas::Contains(const std::string &textureName) const
    {
        return dictionary.find(textureName) != dictionary.end();
    }

    TextureRegion TextureAtlas::GetRegion(const std::string &textureName) const
    {
        auto it = dictionary.find(textureName);

        if (it == dictionary.end())
        {
            spdlog::error("TextureAtlas does not contain texture: {}", textureName);
            throw;
        }

        return it->second;
    }
} // namespace Lucky
