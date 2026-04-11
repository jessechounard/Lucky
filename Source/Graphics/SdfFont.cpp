#include <cmath>

#include <SDL3/SDL_assert.h>
#include <fstream>
#include <stdexcept>

#include <glm/geometric.hpp>
#include <hb.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <Lucky/FileSystem.hpp>
#include <Lucky/SdfFont.hpp>

namespace Lucky {

SdfFont::GlyphData SdfFont::ParseGlyphJson(const void *jsonValue) {
    auto &g = *static_cast<const nlohmann::json *>(jsonValue);
    GlyphData gd{};
    gd.advance = g["advance"].get<float>();
    gd.hasGeometry = g.contains("planeBounds");

    if (gd.hasGeometry) {
        auto &pb = g["planeBounds"];
        gd.planeLeft = pb["left"].get<float>();
        gd.planeTop = pb["top"].get<float>();
        gd.planeRight = pb["right"].get<float>();
        gd.planeBottom = pb["bottom"].get<float>();

        auto &ab = g["atlasBounds"];
        gd.atlasLeft = ab["left"].get<float>();
        gd.atlasTop = ab["top"].get<float>();
        gd.atlasRight = ab["right"].get<float>();
        gd.atlasBottom = ab["bottom"].get<float>();
    }

    return gd;
}

SdfFont::SdfFont(
    GraphicsDevice &graphicsDevice, const std::string &atlasJsonPath, const std::string &ttfPath) {

    // Load HarfBuzz font (initialize all pointers before anything that can throw,
    // so the destructor can safely clean up on partial construction failure)
    hbBlob = hb_blob_create_from_file(ttfPath.c_str());
    if (!hb_blob_get_length(hbBlob)) {
        hb_blob_destroy(hbBlob);
        hbBlob = nullptr;
        throw std::runtime_error("Failed to load font file: " + ttfPath);
    }
    hbFace = hb_face_create(hbBlob, 0);
    hbFont = hb_font_create(hbFace);
    hbBuffer = hb_buffer_create();
    upem = hb_face_get_upem(hbFace);

    // From this point, if anything throws, ~SdfFont must run to clean up HarfBuzz.
    // Wrap the rest in a try/catch to ensure that.
    try {

        // Load atlas JSON
        std::ifstream stream(atlasJsonPath, std::ios::in | std::ios::binary);
        if (!stream) {
            spdlog::error("Failed to load SDF font JSON: {}", atlasJsonPath);
            throw std::runtime_error("Failed to load SDF font JSON: " + atlasJsonPath);
        }

        std::vector<uint8_t> buffer(
            (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (buffer.empty()) {
            spdlog::error("SDF font JSON is empty: {}", atlasJsonPath);
            throw std::runtime_error("SDF font JSON is empty: " + atlasJsonPath);
        }

        nlohmann::json doc;
        try {
            doc = nlohmann::json::parse(buffer);
        } catch (const nlohmann::json::parse_error &) {
            spdlog::error("JSON parse error in SDF font: {}", atlasJsonPath);
            throw std::runtime_error("JSON parse error in SDF font: " + atlasJsonPath);
        }

        // Atlas metadata
        auto &atlas = doc["atlas"];
        pxRange = atlas["distanceRange"].get<float>();
        atlasWidth = atlas["width"].get<float>();
        atlasHeight = atlas["height"].get<float>();

        // The JSON may be in single-font format (top-level "metrics"/"glyphs") or
        // multi-input format with "variants" array (from msdf-atlas-gen -and).
        if (doc.contains("variants")) {
            // Multi-input format: each variant has its own metrics/glyphs
            auto &variants = doc["variants"];
            bool metricsLoaded = false;

            for (auto &variant : variants) {
                if (!metricsLoaded && variant.contains("metrics")) {
                    auto &metrics = variant["metrics"];
                    lineHeight = metrics["lineHeight"].get<float>();
                    ascender = metrics["ascender"].get<float>();
                    descender = metrics["descender"].get<float>();
                    metricsLoaded = true;
                }

                if (variant.contains("glyphs")) {
                    for (auto &g : variant["glyphs"]) {
                        GlyphData gd = ParseGlyphJson(&g);

                        if (g.contains("unicode")) {
                            uint32_t unicode = g["unicode"].get<uint32_t>();
                            hb_codepoint_t glyphId;
                            if (hb_font_get_nominal_glyph(hbFont, unicode, &glyphId)) {
                                glyphs[glyphId] = gd;
                            }
                        } else if (g.contains("index")) {
                            uint32_t glyphId = g["index"].get<uint32_t>();
                            glyphs[glyphId] = gd;
                        }
                    }
                }
            }
        } else {
            // Single-font format (backward compatible)
            auto &metrics = doc["metrics"];
            lineHeight = metrics["lineHeight"].get<float>();
            ascender = metrics["ascender"].get<float>();
            descender = metrics["descender"].get<float>();

            auto &glyphArray = doc["glyphs"];
            for (auto &g : glyphArray) {
                uint32_t unicode = g["unicode"].get<uint32_t>();
                GlyphData gd = ParseGlyphJson(&g);

                hb_codepoint_t glyphId;
                if (hb_font_get_nominal_glyph(hbFont, unicode, &glyphId)) {
                    glyphs[glyphId] = gd;
                }
            }
        }

        // Load atlas texture (derive PNG path from JSON path)
        std::string pngPath = RemoveExtension(atlasJsonPath) + ".png";
        atlasTexture = std::make_unique<Texture>(graphicsDevice, pngPath, TextureFilter::Linear);

        // Load MSDF shader
        std::string basePath = GetBasePath();
        shader = std::make_unique<Shader>(graphicsDevice,
            CombinePaths(basePath, "Content/Shaders/msdf_text.frag"),
            SDL_GPU_SHADERSTAGE_FRAGMENT);

    } catch (...) {
        if (hbBuffer)
            hb_buffer_destroy(hbBuffer);
        if (hbFont)
            hb_font_destroy(hbFont);
        if (hbFace)
            hb_face_destroy(hbFace);
        if (hbBlob)
            hb_blob_destroy(hbBlob);
        throw;
    }
}

void SdfFont::ShapeText(const std::string &text) {
    hb_buffer_clear_contents(hbBuffer);
    hb_buffer_add_utf8(hbBuffer, text.c_str(), (int)text.size(), 0, (int)text.size());
    hb_buffer_guess_segment_properties(hbBuffer);
    hb_shape(hbFont, hbBuffer, nullptr, 0);
}

SdfFont::~SdfFont() {
    if (hbBuffer)
        hb_buffer_destroy(hbBuffer);
    if (hbFont)
        hb_font_destroy(hbFont);
    if (hbFace)
        hb_face_destroy(hbFace);
    if (hbBlob)
        hb_blob_destroy(hbBlob);
}

void SdfFont::EmitGlyphQuad(BatchRenderer &batchRenderer, const GlyphData &gd, float cursorX,
    float baselineY, float scale, const TextEffect &effect, Color color) {
    float planeW = gd.planeRight - gd.planeLeft;
    float planeH = gd.planeBottom - gd.planeTop;

    if (planeW <= 0.0f || planeH <= 0.0f)
        return;

    float halfRange = pxRange / 2.0f;
    float atlasPerEmX = (gd.atlasRight - gd.atlasLeft) / planeW;
    float atlasPerEmY = (gd.atlasBottom - gd.atlasTop) / planeH;
    float emPadX = halfRange / atlasPerEmX;
    float emPadY = halfRange / atlasPerEmY;

    float shapeLeft = gd.planeLeft + emPadX;
    float shapeRight = gd.planeRight - emPadX;
    float shapeTop = gd.planeTop + emPadY;
    float shapeBottom = gd.planeBottom - emPadY;
    float shapeAtlasLeft = gd.atlasLeft + halfRange;
    float shapeAtlasRight = gd.atlasRight - halfRange;
    float shapeAtlasTop = gd.atlasTop + halfRange;
    float shapeAtlasBottom = gd.atlasBottom - halfRange;

    float effectExtent = effect.outlineThickness + effect.glowThickness + effect.glowSoftness;
    float padEm = std::max(1.0f, effectExtent + 1.0f) / scale;
    float padAtlasX = std::min(padEm * atlasPerEmX, halfRange);
    float padAtlasY = std::min(padEm * atlasPerEmY, halfRange);
    float actualPadEmX = padAtlasX / atlasPerEmX;
    float actualPadEmY = padAtlasY / atlasPerEmY;

    float invW = 1.0f / atlasWidth;
    float invH = 1.0f / atlasHeight;

    glm::vec2 xy0{cursorX + (shapeLeft - actualPadEmX) * scale,
        baselineY - (shapeBottom + actualPadEmY) * scale};
    glm::vec2 xy1{cursorX + (shapeRight + actualPadEmX) * scale,
        baselineY - (shapeTop - actualPadEmY) * scale};
    glm::vec2 uv0{(shapeAtlasLeft - padAtlasX) * invW, (shapeAtlasBottom + padAtlasY) * invH};
    glm::vec2 uv1{(shapeAtlasRight + padAtlasX) * invW, (shapeAtlasTop - padAtlasY) * invH};

    batchRenderer.BatchQuadUV(uv0, uv1, xy0, xy1, color);
}

// --- Batched API ---

void SdfFont::BeginBatch(BatchRenderer &batchRenderer, const TextEffect &effect) {
    batchEffect = effect;

    batchParams = {};
    batchParams.pxRange = pxRange;
    batchParams.atlasWidth = atlasWidth;
    batchParams.atlasHeight = atlasHeight;
    batchParams.outlineThickness = effect.outlineThickness;
    batchParams.outlineColorR = effect.outlineColor.r;
    batchParams.outlineColorG = effect.outlineColor.g;
    batchParams.outlineColorB = effect.outlineColor.b;
    batchParams.outlineColorA = effect.outlineColor.a;
    batchParams.glowThickness = effect.glowThickness;
    batchParams.glowSoftness = effect.glowSoftness;
    batchParams.glowColorR = effect.glowColor.r;
    batchParams.glowColorG = effect.glowColor.g;
    batchParams.glowColorB = effect.glowColor.b;
    batchParams.glowColorA = effect.glowColor.a;

    batchRenderer.Begin(BlendMode::Alpha, *atlasTexture, *shader);
    batchRenderer.SetFragmentUniformData(&batchParams, sizeof(batchParams));
}

void SdfFont::EndBatch(BatchRenderer &batchRenderer) {
    batchRenderer.End();
}

void SdfFont::DrawStringBatched(BatchRenderer &batchRenderer, const std::string &text,
    glm::vec2 position, float fontSize, Color color) {
    if (text.empty())
        return;
    SDL_assert(fontSize > 0.0f);

    float scale = fontSize;
    float hbScale = scale / static_cast<float>(upem);

    ShapeText(text);

    unsigned int glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

    float baselineY = position.y;
    float cursorX = position.x;

    for (unsigned int i = 0; i < glyphCount; i++) {
        uint32_t glyphId = glyphInfo[i].codepoint;

        if (glyphId == 0) {
            uint32_t cluster = glyphInfo[i].cluster;
            if (cluster < text.size() && text[cluster] == '\n') {
                cursorX = position.x;
                baselineY -= lineHeight * scale;
                continue;
            }
        }

        float xOffset = glyphPos[i].x_offset * hbScale;
        float yOffset = glyphPos[i].y_offset * hbScale;
        float xAdvance = glyphPos[i].x_advance * hbScale;
        float yAdvance = glyphPos[i].y_advance * hbScale;

        auto it = glyphs.find(glyphId);
        if (it != glyphs.end() && it->second.hasGeometry) {
            EmitGlyphQuad(batchRenderer,
                it->second,
                cursorX + xOffset,
                baselineY + yOffset,
                scale,
                batchEffect,
                color);
        }

        cursorX += xAdvance;
        baselineY += yAdvance;
    }
}

void SdfFont::DrawStringBatched(BatchRenderer &batchRenderer, const std::string &text,
    const std::function<glm::vec2(float)> &path, float fontSize, Color color) {
    if (text.empty())
        return;
    SDL_assert(fontSize > 0.0f);

    float scale = fontSize;
    float hbScale = scale / static_cast<float>(upem);
    float invW = 1.0f / atlasWidth;
    float invH = 1.0f / atlasHeight;

    ShapeText(text);

    unsigned int glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

    float distance = 0;
    constexpr float epsilon = 0.5f;
    const TextEffect &effect = batchEffect;

    for (unsigned int i = 0; i < glyphCount; i++) {
        uint32_t glyphId = glyphInfo[i].codepoint;
        float xAdvance = glyphPos[i].x_advance * hbScale;

        auto it = glyphs.find(glyphId);
        if (it == glyphs.end()) {
            distance += xAdvance;
            continue;
        }

        const GlyphData &gd = it->second;

        if (gd.hasGeometry) {
            float planeW = gd.planeRight - gd.planeLeft;
            float planeH = gd.planeBottom - gd.planeTop;

            if (planeW > 0.0f && planeH > 0.0f) {
                float centerDist = distance + xAdvance * 0.5f;

                glm::vec2 pos = path(centerDist);
                glm::vec2 forward = path(centerDist + epsilon) - pos;
                float forwardLen = glm::length(forward);
                glm::vec2 tangent = (forwardLen > 1e-6f) ? forward / forwardLen : glm::vec2{1, 0};
                glm::vec2 normal = {-tangent.y, tangent.x};

                float halfRange = pxRange / 2.0f;
                float atlasPerEmX = (gd.atlasRight - gd.atlasLeft) / planeW;
                float atlasPerEmY = (gd.atlasBottom - gd.atlasTop) / planeH;
                float emPadX = halfRange / atlasPerEmX;
                float emPadY = halfRange / atlasPerEmY;

                float shapeLeft = gd.planeLeft + emPadX;
                float shapeRight = gd.planeRight - emPadX;
                float shapeTop = gd.planeTop + emPadY;
                float shapeBottom = gd.planeBottom - emPadY;
                float shapeAtlasLeft = gd.atlasLeft + halfRange;
                float shapeAtlasRight = gd.atlasRight - halfRange;
                float shapeAtlasTop = gd.atlasTop + halfRange;
                float shapeAtlasBottom = gd.atlasBottom - halfRange;

                float effectExtent =
                    effect.outlineThickness + effect.glowThickness + effect.glowSoftness;
                float padEm = std::max(1.0f, effectExtent + 1.0f) / scale;
                float padAtlasX = std::min(padEm * atlasPerEmX, halfRange);
                float padAtlasY = std::min(padEm * atlasPerEmY, halfRange);
                float actualPadEmX = padAtlasX / atlasPerEmX;
                float actualPadEmY = padAtlasY / atlasPerEmY;

                float halfAdv = xAdvance / scale * 0.5f;
                float localLeft = (shapeLeft - actualPadEmX - halfAdv) * scale;
                float localRight = (shapeRight + actualPadEmX - halfAdv) * scale;
                float localBottom = -(shapeBottom + actualPadEmY) * scale;
                float localTop = -(shapeTop - actualPadEmY) * scale;

                glm::vec2 corners[4] = {
                    pos + tangent * localLeft + normal * localBottom,
                    pos + tangent * localRight + normal * localBottom,
                    pos + tangent * localRight + normal * localTop,
                    pos + tangent * localLeft + normal * localTop,
                };

                glm::vec2 uvs[4] = {
                    {(shapeAtlasLeft - padAtlasX) * invW, (shapeAtlasBottom + padAtlasY) * invH},
                    {(shapeAtlasRight + padAtlasX) * invW, (shapeAtlasBottom + padAtlasY) * invH},
                    {(shapeAtlasRight + padAtlasX) * invW, (shapeAtlasTop - padAtlasY) * invH},
                    {(shapeAtlasLeft - padAtlasX) * invW, (shapeAtlasTop - padAtlasY) * invH},
                };

                batchRenderer.BatchQuad(corners, uvs, color);
            }
        }

        distance += xAdvance;
    }
}

// --- Convenience API (self-contained, one draw call per string) ---

void SdfFont::DrawString(BatchRenderer &batchRenderer, const std::string &text, glm::vec2 position,
    float fontSize, Color color, const TextEffect &effect) {
    if (text.empty())
        return;

    if (batchRenderer.BatchStarted()) {
        spdlog::error("SdfFont::DrawString called while a batch is already active.");
        return;
    }

    BeginBatch(batchRenderer, effect);
    DrawStringBatched(batchRenderer, text, position, fontSize, color);
    EndBatch(batchRenderer);
}

void SdfFont::DrawString(BatchRenderer &batchRenderer, const std::string &text,
    const std::function<glm::vec2(float)> &path, float fontSize, Color color,
    const TextEffect &effect) {
    if (text.empty())
        return;

    if (batchRenderer.BatchStarted()) {
        spdlog::error("SdfFont::DrawString called while a batch is already active.");
        return;
    }

    BeginBatch(batchRenderer, effect);
    DrawStringBatched(batchRenderer, text, path, fontSize, color);
    EndBatch(batchRenderer);
}

glm::vec2 SdfFont::MeasureString(const std::string &text, float fontSize) {
    if (text.empty())
        return {0, 0};
    SDL_assert(fontSize > 0.0f);

    float scale = fontSize;
    float hbScale = scale / static_cast<float>(upem);

    ShapeText(text);

    unsigned int glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

    float maxWidth = 0;
    float cursorX = 0;
    int lineCount = 1;

    for (unsigned int i = 0; i < glyphCount; i++) {
        uint32_t glyphId = glyphInfo[i].codepoint;

        if (glyphId == 0) {
            uint32_t cluster = glyphInfo[i].cluster;
            if (cluster < text.size() && text[cluster] == '\n') {
                maxWidth = std::max(maxWidth, cursorX);
                cursorX = 0;
                ++lineCount;
                continue;
            }
        }

        cursorX += glyphPos[i].x_advance * hbScale;
    }

    maxWidth = std::max(maxWidth, cursorX);
    return {maxWidth, lineCount * lineHeight * scale};
}

float SdfFont::GetLineHeight(float fontSize) {
    return lineHeight * fontSize;
}

float SdfFont::GetAscender(float fontSize) {
    return -ascender * fontSize;
}

float SdfFont::GetDescender(float fontSize) {
    return descender * fontSize;
}

} // namespace Lucky
