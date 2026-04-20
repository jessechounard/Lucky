#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <Lucky/TextureAtlas.hpp>

using namespace Lucky;

namespace {

// Helper that wraps a JSON string literal and hands it to TextureAtlas's
// memory constructor.
TextureAtlas ParseAtlas(const std::string &json, const std::string &fileName = "") {
    std::vector<uint8_t> buffer(json.begin(), json.end());
    return TextureAtlas(buffer.data(), buffer.size(), fileName);
}

} // namespace

TEST_CASE("TextureAtlas parses a simple TexturePacker JSON") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "idle",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            },
            {
                "filename": "run",
                "frame": {"x": 32, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            }
        ],
        "meta": {
            "image": "player.png"
        }
    })";

    TextureAtlas atlas = ParseAtlas(json);

    CHECK(atlas.Regions().size() == 2);
    CHECK(atlas.Contains("idle"));
    CHECK(atlas.Contains("run"));
    CHECK_FALSE(atlas.Contains("missing"));
}

TEST_CASE("TextureAtlas reads frame bounds") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "idle",
                "frame": {"x": 5, "y": 10, "w": 32, "h": 48},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 48},
                "sourceSize": {"w": 32, "h": 48}
            }
        ],
        "meta": {"image": "player.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    TextureRegion region = atlas.GetRegion("idle");

    CHECK(region.bounds.x == 5);
    CHECK(region.bounds.y == 10);
    CHECK(region.bounds.width == 32);
    CHECK(region.bounds.height == 48);
    CHECK_FALSE(region.rotated);
}

TEST_CASE("TextureAtlas reads the rotated flag") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "tilted",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": true,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            }
        ],
        "meta": {"image": "tiles.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    CHECK(atlas.GetRegion("tilted").rotated);
}

TEST_CASE("TextureAtlas computes origin from un-trimmed sprites") {
    // A sprite where spriteSourceSize matches sourceSize exactly means no
    // trimming. originTopLeft should be (0, 0) and originBottomRight (1, 1).
    const std::string json = R"({
        "frames": [
            {
                "filename": "full",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            }
        ],
        "meta": {"image": "player.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    TextureRegion region = atlas.GetRegion("full");

    CHECK(region.originTopLeft.x == doctest::Approx(0.0f));
    CHECK(region.originTopLeft.y == doctest::Approx(0.0f));
    CHECK(region.originBottomRight.x == doctest::Approx(1.0f));
    CHECK(region.originBottomRight.y == doctest::Approx(1.0f));
}

TEST_CASE("TextureAtlas reads pivot when present") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "pivoted",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32},
                "pivot": {"x": 0.5, "y": 1.0}
            }
        ],
        "meta": {"image": "player.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    TextureRegion region = atlas.GetRegion("pivoted");

    // Pivot (0.5, 1.0) on an un-trimmed sprite lerps between
    // originTopLeft=(0,0) and originBottomRight=(1,1).
    CHECK(region.pivot.x == doctest::Approx(0.5f));
    CHECK(region.pivot.y == doctest::Approx(1.0f));
}

TEST_CASE("TextureAtlas defaults pivot to center when missing") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "nopivot",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            }
        ],
        "meta": {"image": "player.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    TextureRegion region = atlas.GetRegion("nopivot");

    // Missing pivot falls back to (0.5, 0.5), which for an un-trimmed sprite
    // is the center of the region.
    CHECK(region.pivot.x == doctest::Approx(0.5f));
    CHECK(region.pivot.y == doctest::Approx(0.5f));
}

TEST_CASE("TextureAtlas resolves texturePath relative to the JSON file") {
    const std::string json = R"({
        "frames": [],
        "meta": {"image": "player.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json, "Content/Atlases/player.json");
    CHECK(atlas.TexturePath() == "Content/Atlases/player.png");
}

TEST_CASE("TextureAtlas texturePath has no leading path when JSON has none") {
    const std::string json = R"({
        "frames": [],
        "meta": {"image": "player.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json, "player.json");
    CHECK(atlas.TexturePath() == "player.png");
}

TEST_CASE("TextureAtlas throws on malformed JSON") {
    const std::string json = "this is not valid json";
    CHECK_THROWS_AS(ParseAtlas(json), std::runtime_error);
}

TEST_CASE("TextureAtlas throws when frames field is missing") {
    const std::string json = R"({"meta": {"image": "player.png"}})";
    CHECK_THROWS_AS(ParseAtlas(json), std::runtime_error);
}

TEST_CASE("TextureAtlas throws when meta.image is missing") {
    const std::string json = R"({"frames": [], "meta": {}})";
    CHECK_THROWS_AS(ParseAtlas(json), std::runtime_error);
}

TEST_CASE("TextureAtlas throws when a frame lacks required subfields") {
    const std::string json = R"({
        "frames": [
            {"filename": "partial", "frame": {"x": 0, "y": 0, "w": 32, "h": 32}}
        ],
        "meta": {"image": "player.png"}
    })";
    CHECK_THROWS_AS(ParseAtlas(json), std::runtime_error);
}

TEST_CASE("TextureAtlas throws when spriteSourceSize is zero") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "degenerate",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 0, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            }
        ],
        "meta": {"image": "player.png"}
    })";
    CHECK_THROWS_AS(ParseAtlas(json), std::runtime_error);
}

TEST_CASE("GetRegion throws for missing names") {
    const std::string json = R"({
        "frames": [],
        "meta": {"image": "player.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    CHECK_THROWS_AS(atlas.GetRegion("nothing"), std::runtime_error);
}

TEST_CASE("ContainsNoExt matches on filename without extension") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "player.png",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            }
        ],
        "meta": {"image": "sheet.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    CHECK(atlas.ContainsNoExt("player"));
    CHECK(atlas.ContainsNoExt("player.png"));
    CHECK_FALSE(atlas.ContainsNoExt("missing"));
}

TEST_CASE("GetRegionNoExt matches on filename without extension") {
    const std::string json = R"({
        "frames": [
            {
                "filename": "player.png",
                "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
                "rotated": false,
                "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
                "sourceSize": {"w": 32, "h": 32}
            }
        ],
        "meta": {"image": "sheet.png"}
    })";

    TextureAtlas atlas = ParseAtlas(json);
    // Both the extension-less name and the extension-ful name should find
    // the region.
    TextureRegion byExt = atlas.GetRegion("player.png");
    TextureRegion byNoExt = atlas.GetRegionNoExt("player");
    CHECK(byExt.bounds.x == byNoExt.bounds.x);
    CHECK(byExt.bounds.width == byNoExt.bounds.width);
}
