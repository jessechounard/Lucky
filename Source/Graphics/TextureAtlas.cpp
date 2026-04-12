#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <Lucky/TextureAtlas.hpp>
#include <Lucky/MathHelpers.hpp>

namespace Lucky {

TextureAtlas::TextureAtlas(const std::string &fileName) {
    std::ifstream stream(fileName, std::ios::in | std::ios::binary);
    if (!stream) {
        spdlog::error("Failed to load texture dictionary file: {}", fileName);
        throw std::runtime_error("Failed to load texture dictionary file: " + fileName);
    }

    std::vector<uint8_t> buffer(
        (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (buffer.size() == 0) {
        spdlog::error("Failed to read texture dictionary: {}", fileName);
        throw std::runtime_error("Failed to read texture dictionary: " + fileName);
    }

    Initialize(buffer.data(), buffer.size(), fileName);
}

TextureAtlas::TextureAtlas(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName) {
    Initialize(buffer, bufferLength, fileName);
}

void TextureAtlas::Initialize(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName) {
    nlohmann::json jsonDocument;

    try {
        jsonDocument = nlohmann::json::parse(buffer, buffer + bufferLength);
    } catch (const nlohmann::json::parse_error &) {
        spdlog::error("JSON parsing failed for file: {}", fileName);
        throw std::runtime_error("JSON parsing failed for file: " + fileName);
    }

    auto &frames = jsonDocument["frames"];

    for (auto &frame : frames) {
        std::string name = frame["filename"].get<std::string>();

        TextureRegion textureRegion;
        textureRegion.bounds.x = frame["frame"]["x"].get<int>();
        textureRegion.bounds.y = frame["frame"]["y"].get<int>();
        textureRegion.bounds.width = frame["frame"]["w"].get<int>();
        textureRegion.bounds.height = frame["frame"]["h"].get<int>();
        textureRegion.rotated = frame["rotated"].get<bool>();

        float spriteSourceSizeX = frame["spriteSourceSize"]["x"].get<float>();
        float spriteSourceSizeY = frame["spriteSourceSize"]["y"].get<float>();
        float spriteSourceSizeW = frame["spriteSourceSize"]["w"].get<float>();
        float spriteSourceSizeH = frame["spriteSourceSize"]["h"].get<float>();

        float sourceSizeW = frame["sourceSize"]["w"].get<float>();
        float sourceSizeH = frame["sourceSize"]["h"].get<float>();

        textureRegion.originTopLeft.x = -spriteSourceSizeX / spriteSourceSizeW;
        textureRegion.originTopLeft.y = -spriteSourceSizeY / spriteSourceSizeH;

        textureRegion.originBottomRight.x =
            1 + (sourceSizeW - (spriteSourceSizeX + spriteSourceSizeW)) / spriteSourceSizeW;
        textureRegion.originBottomRight.y =
            1 + (sourceSizeH - (spriteSourceSizeY + spriteSourceSizeH)) / spriteSourceSizeH;

        textureRegion.originCenter =
            (textureRegion.originTopLeft + textureRegion.originBottomRight) / 2.0f;

        glm::vec2 pivot = {0.5f, 0.5f};
        if (frame.contains("pivot")) {
            pivot.x = frame["pivot"]["x"].get<float>();
            pivot.y = frame["pivot"]["y"].get<float>();
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
    texturePath = (std::filesystem::path(fileName).parent_path() /
                      meta["image"].get<std::string>())
                      .generic_string();
}

bool TextureAtlas::Contains(const std::string &textureName) const {
    return dictionary.find(textureName) != dictionary.end();
}

TextureRegion TextureAtlas::GetRegion(const std::string &textureName) const {
    auto it = dictionary.find(textureName);

    if (it == dictionary.end()) {
        spdlog::error("TextureAtlas does not contain texture: {}", textureName);
        throw std::runtime_error("TextureAtlas does not contain texture: " + textureName);
    }

    return it->second;
}

TextureRegion TextureAtlas::GetRegionNoExt(const std::string &textureName) const {
    std::filesystem::path textureNameNoExt =
        std::filesystem::path(textureName).replace_extension();

    for (const auto &kvp : dictionary) {
        if (std::filesystem::path(kvp.first).replace_extension() == textureNameNoExt) {
            return kvp.second;
        }
    }

    spdlog::error("TextureAtlas::GetRegionNoExt: Could not find region {}", textureName);
    throw std::runtime_error("TextureAtlas::GetRegionNoExt: Could not find region " + textureName);
}

} // namespace Lucky
