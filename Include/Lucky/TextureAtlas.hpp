#pragma once

#include <map>
#include <stdint.h>
#include <string>

#include <glm/glm.hpp>

#include <Lucky/Rectangle.hpp>

namespace Lucky {

/**
 * A named region within a texture atlas.
 *
 * Describes one sprite packed into a larger atlas texture, including where
 * the sprite sits in the atlas image, where its trimmed content sits
 * relative to its original untrimmed bounds, and where its pivot is.
 *
 * # Origin coordinates
 *
 * Atlas packers typically trim transparent pixels off each sprite before
 * packing them, so a sprite's visible pixels may occupy a smaller rectangle
 * than its original size. `originTopLeft` and `originBottomRight` describe
 * where the untrimmed sprite's corners would fall if `(0, 0)`-`(1, 1)`
 * described the trimmed region. For a sprite with no trimming these are
 * `(0, 0)` and `(1, 1)` exactly. For a trimmed sprite they extend outside
 * `[0, 1]` in whichever directions pixels were cropped, so a renderer can
 * offset and scale the quad to reproduce the original silhouette.
 *
 * # Pivot
 *
 * `pivot` is the sprite's rotation/scale anchor in the same origin-space
 * coordinates as the corners. When the atlas JSON omits a pivot, it
 * defaults to the centre `(0.5, 0.5)`.
 *
 * # Rotation
 *
 * `rotated` is set by the packer when it rotates a sprite 90° clockwise to
 * pack it more tightly. Consumers must rotate the sampled UVs accordingly
 * when drawing.
 */
struct TextureRegion {
    Rectangle bounds; // the sprite's rectangle in the atlas image, in atlas pixels

    glm::vec2 originTopLeft;     // untrimmed top-left in origin space (0, 0 when un-trimmed)
    glm::vec2 originBottomRight; // untrimmed bottom-right in origin space (1, 1 when un-trimmed)
    glm::vec2 originCenter;      // midpoint of originTopLeft and originBottomRight
    glm::vec2 pivot;             // pivot in origin space; defaults to (0.5, 0.5) if JSON omits it

    bool rotated; // true if the sprite is rotated 90° clockwise in the atlas
};

/**
 * A collection of named `TextureRegion`s packed into a single atlas image.
 *
 * Loads the JSON output produced by TexturePacker's "JSON (Array)" preset
 * and builds a name-to-region map. The atlas does not load the image itself;
 * `TexturePath()` returns the path to the companion image that the caller is
 * expected to load separately with a `Texture`.
 *
 * The "JSON (Hash)" preset — where `frames` is an object keyed by filename
 * — is not supported; export as "JSON (Array)" instead.
 *
 * # Usage
 *
 * Load from a JSON file:
 *
 *     TextureAtlas atlas("Content/Atlases/hero.json");
 *     Texture texture(graphicsDevice, atlas.TexturePath());
 *     TextureRegion idle = atlas.GetRegion("idle.png");
 *
 * Or from an in-memory JSON buffer (for embedded assets):
 *
 *     TextureAtlas atlas(buffer, bufferLength, "hero.json");
 *
 * # JSON format
 *
 * The document must have a top-level `frames` array (not object) and a
 * `meta` object. Each entry in `frames` must supply `filename`, `frame`
 * (`x`, `y`, `w`, `h`), `rotated`, `spriteSourceSize` (`x`, `y`, `w`, `h`)
 * with positive width and height, and `sourceSize` (`w`, `h`). An optional
 * `pivot` object (`x`, `y`) sets the sprite's pivot; otherwise the pivot
 * defaults to the centre. `meta.image` names the companion image, resolved
 * relative to the JSON file's directory.
 *
 * When a sprite was rotated by the packer, `rotated` is `true` and the
 * `frame.w`/`h` values describe the sprite's **original** (pre-rotation)
 * dimensions; the atlas image itself stores the pixels rotated 90°
 * clockwise. This matches TexturePacker's "JSON (Array)" output.
 *
 * Malformed JSON, missing required fields, or non-positive
 * `spriteSourceSize` values cause the constructor to throw
 * `std::runtime_error` with a message that names the offending atlas.
 */
struct TextureAtlas {
  public:
    /**
     * Loads and parses a texture atlas from a JSON file on disk.
     *
     * \param fileName path to the JSON file. `TexturePath()` after
     *                 construction will be the `meta.image` value joined
     *                 against this file's parent directory.
     * \throws std::runtime_error if the file cannot be opened, is empty,
     *                            has malformed JSON, is missing required
     *                            fields, or contains a frame with
     *                            non-positive `spriteSourceSize`.
     */
    TextureAtlas(const std::string &fileName);

    /**
     * Parses a texture atlas from an in-memory JSON buffer.
     *
     * \param buffer pointer to the JSON bytes.
     * \param bufferLength length of the buffer in bytes.
     * \param fileName optional name used only to resolve `meta.image` into
     *                 `TexturePath()` and to label parse errors. When empty,
     *                 `TexturePath()` returns `meta.image` unchanged.
     * \throws std::runtime_error on malformed JSON, missing required
     *                            fields, or non-positive
     *                            `spriteSourceSize`.
     */
    TextureAtlas(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName = "");

    /**
     * Returns true if the atlas contains a region with the exact given name.
     *
     * Matches are case-sensitive and must include the file extension if the
     * atlas entry has one (`"idle.png"`, not `"idle"`).
     *
     * \param textureName the region name to look up.
     */
    bool Contains(const std::string &textureName) const;

    /**
     * Returns true if the atlas contains a region whose name matches
     * `textureName` ignoring file extensions.
     *
     * Both the query and each stored name have their extensions stripped
     * before comparison, so `ContainsNoExt("idle")` matches an atlas entry
     * stored as `"idle.png"`.
     *
     * \note If the atlas contains multiple regions whose names differ only
     *       in extension (for example `"foo.png"` and `"foo.bmp"`), both
     *       will match this query but only one can be fetched by
     *       `GetRegionNoExt()`. Callers that need an unambiguous lookup
     *       should use `Contains()` / `GetRegion()` with the full name.
     *
     * \param textureName the region name to look up. Extension is ignored.
     */
    bool ContainsNoExt(const std::string &textureName) const;

    /**
     * Returns the region stored under the exact given name.
     *
     * \param textureName the region name to look up, including extension.
     * \returns the matching `TextureRegion`.
     * \throws std::runtime_error if no region with that exact name exists.
     */
    TextureRegion GetRegion(const std::string &textureName) const;

    /**
     * Returns the region whose stored name matches `textureName` ignoring
     * file extensions.
     *
     * \note If the atlas contains multiple regions whose names differ only
     *       in extension, this returns an arbitrary match (whichever sorts
     *       first in the map). Callers that need a specific entry should
     *       use `GetRegion()` with the full name.
     *
     * \param textureName the region name to look up. Extension is ignored.
     * \returns the matching `TextureRegion`.
     * \throws std::runtime_error if no region matches.
     */
    TextureRegion GetRegionNoExt(const std::string &textureName) const;

    /**
     * Returns the path to the companion atlas image, resolved relative to
     * the JSON file's directory.
     *
     * Usually passed directly to a `Texture` constructor to load the image.
     */
    const std::string &TexturePath() const {
        return texturePath;
    }

    /**
     * Returns the full name-to-region map.
     *
     * Useful for iteration, debugging, or exporting. The returned reference
     * is valid for the lifetime of this atlas.
     */
    const std::map<std::string, TextureRegion> &Regions() const {
        return dictionary;
    }

  private:
    void Initialize(uint8_t *buffer, uint64_t bufferLength, const std::string &fileName);

    std::string texturePath;
    std::map<std::string, TextureRegion> dictionary;
};

} // namespace Lucky
