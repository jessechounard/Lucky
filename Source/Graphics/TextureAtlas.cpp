#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <Lucky/MathHelpers.hpp>
#include <Lucky/TextureAtlas.hpp>

namespace Lucky {

namespace {

std::string DescribeSource(const std::string &fileName) {
    return fileName.empty() ? "<memory>" : fileName;
}

} // namespace

TextureAtlas::TextureAtlas(const std::string &fileName) {
    std::ifstream stream(fileName, std::ios::in | std::ios::binary);
    if (!stream) {
        spdlog::error("Failed to load texture atlas file: {}", fileName);
        throw std::runtime_error("Failed to load texture atlas file: " + fileName);
    }

    std::vector<uint8_t> buffer(
        (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (buffer.size() == 0) {
        spdlog::error("Texture atlas file is empty: {}", fileName);
        throw std::runtime_error("Texture atlas file is empty: " + fileName);
    }

    Initialize(buffer.data(), buffer.size(), fileName);
}

TextureAtlas::TextureAtlas(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName) {
    Initialize(buffer, bufferLength, fileName);
}

void TextureAtlas::Initialize(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName) {
    const std::string source = DescribeSource(fileName);

    nlohmann::json jsonDocument;
    try {
        jsonDocument = nlohmann::json::parse(buffer, buffer + bufferLength);
    } catch (const nlohmann::json::parse_error &) {
        spdlog::error("JSON parsing failed for texture atlas: {}", source);
        throw std::runtime_error("JSON parsing failed for texture atlas: " + source);
    }

    try {
        auto &frames = jsonDocument.at("frames");

        for (auto &frame : frames) {
            std::string name = frame.at("filename").get<std::string>();

            TextureRegion textureRegion;
            textureRegion.bounds.x = frame.at("frame").at("x").get<int>();
            textureRegion.bounds.y = frame.at("frame").at("y").get<int>();
            textureRegion.bounds.width = frame.at("frame").at("w").get<int>();
            textureRegion.bounds.height = frame.at("frame").at("h").get<int>();
            textureRegion.rotated = frame.at("rotated").get<bool>();

            float spriteSourceSizeX = frame.at("spriteSourceSize").at("x").get<float>();
            float spriteSourceSizeY = frame.at("spriteSourceSize").at("y").get<float>();
            float spriteSourceSizeW = frame.at("spriteSourceSize").at("w").get<float>();
            float spriteSourceSizeH = frame.at("spriteSourceSize").at("h").get<float>();

            float sourceSizeW = frame.at("sourceSize").at("w").get<float>();
            float sourceSizeH = frame.at("sourceSize").at("h").get<float>();

            if (spriteSourceSizeW <= 0.0f || spriteSourceSizeH <= 0.0f) {
                spdlog::error("Texture atlas frame '{}' has non-positive spriteSourceSize in {}",
                    name,
                    source);
                throw std::runtime_error("Texture atlas frame '" + name +
                                         "' has non-positive spriteSourceSize in " + source);
            }

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
                pivot.x = frame.at("pivot").at("x").get<float>();
                pivot.y = frame.at("pivot").at("y").get<float>();
            }

            textureRegion.pivot.x = Lerp<float>(
                textureRegion.originTopLeft.x, textureRegion.originBottomRight.x, pivot.x);
            textureRegion.pivot.y = Lerp<float>(
                textureRegion.originTopLeft.y, textureRegion.originBottomRight.y, pivot.y);

            dictionary[name] = textureRegion;
        }

        auto &meta = jsonDocument.at("meta");
        texturePath =
            (std::filesystem::path(fileName).parent_path() / meta.at("image").get<std::string>())
                .generic_string();
    } catch (const nlohmann::json::exception &e) {
        spdlog::error("Texture atlas '{}' is malformed: {}", source, e.what());
        throw std::runtime_error("Texture atlas '" + source + "' is malformed: " + e.what());
    }
}

bool TextureAtlas::Contains(const std::string &textureName) const {
    return dictionary.find(textureName) != dictionary.end();
}

bool TextureAtlas::ContainsNoExt(const std::string &textureName) const {
    std::filesystem::path textureNameNoExt = std::filesystem::path(textureName).replace_extension();

    for (const auto &kvp : dictionary) {
        if (std::filesystem::path(kvp.first).replace_extension() == textureNameNoExt) {
            return true;
        }
    }

    return false;
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
    std::filesystem::path textureNameNoExt = std::filesystem::path(textureName).replace_extension();

    for (const auto &kvp : dictionary) {
        if (std::filesystem::path(kvp.first).replace_extension() == textureNameNoExt) {
            return kvp.second;
        }
    }

    spdlog::error("TextureAtlas::GetRegionNoExt: Could not find region {}", textureName);
    throw std::runtime_error("TextureAtlas::GetRegionNoExt: Could not find region " + textureName);
}

} // namespace Lucky
