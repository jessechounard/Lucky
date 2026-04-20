#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <Lucky/SpriteAnimation.hpp>
#include <Lucky/TextureAtlas.hpp>

using namespace Lucky;

namespace {

// Produces a TextureAtlas with three named frames: "a", "b", "c".
TextureAtlas MakeThreeFrameAtlas() {
    const std::string json = R"({
        "frames": [
            {"filename": "a",
             "frame": {"x": 0, "y": 0, "w": 10, "h": 10},
             "rotated": false,
             "spriteSourceSize": {"x": 0, "y": 0, "w": 10, "h": 10},
             "sourceSize": {"w": 10, "h": 10}},
            {"filename": "b",
             "frame": {"x": 10, "y": 0, "w": 10, "h": 10},
             "rotated": false,
             "spriteSourceSize": {"x": 0, "y": 0, "w": 10, "h": 10},
             "sourceSize": {"w": 10, "h": 10}},
            {"filename": "c",
             "frame": {"x": 20, "y": 0, "w": 10, "h": 10},
             "rotated": false,
             "spriteSourceSize": {"x": 0, "y": 0, "w": 10, "h": 10},
             "sourceSize": {"w": 10, "h": 10}}
        ],
        "meta": {"image": "sheet.png"}
    })";

    std::vector<uint8_t> buffer(json.begin(), json.end());
    return TextureAtlas(buffer.data(), buffer.size());
}

std::shared_ptr<SpriteAnimation> MakeAnimation(TextureAtlas &atlas, float duration, bool loop) {
    return std::make_shared<SpriteAnimation>(
        atlas, std::vector<std::string>{"a", "b", "c"}, duration, loop);
}

} // namespace

TEST_CASE("SpriteAnimation records frames in order") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimation anim(atlas, {"c", "a", "b"}, 1.0f, true);

    REQUIRE(anim.Frames().size() == 3);
    CHECK(anim.Frames()[0].bounds.x == 20); // "c"
    CHECK(anim.Frames()[1].bounds.x == 0);  // "a"
    CHECK(anim.Frames()[2].bounds.x == 10); // "b"
    CHECK(anim.GetDuration() == doctest::Approx(1.0f));
    CHECK(anim.IsLooping());
}

TEST_CASE("SpriteAnimationPlayer with no current animation stays not-complete") {
    SpriteAnimationPlayer player;
    CHECK_FALSE(player.IsComplete());
    CHECK(player.GetTime() == doctest::Approx(0.0f));
}

TEST_CASE("SpriteAnimationPlayer Update with no current animation is a no-op") {
    SpriteAnimationPlayer player;
    player.Update(1.0f);
    CHECK(player.GetTime() == doctest::Approx(0.0f));
}

TEST_CASE("Play starts time at zero by default") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("run", MakeAnimation(atlas, 3.0f, true));

    player.Play("run");
    CHECK(player.GetTime() == doctest::Approx(0.0f));
}

TEST_CASE("Play respects startOffset on a looping animation") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("run", MakeAnimation(atlas, 3.0f, true));

    // startOffset greater than duration should wrap via fmod.
    player.Play("run", 7.5f);
    CHECK(player.GetTime() == doctest::Approx(1.5f));
}

TEST_CASE("Play clamps startOffset on a non-looping animation") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("once", MakeAnimation(atlas, 3.0f, false));

    player.Play("once", 100.0f);
    CHECK(player.GetTime() == doctest::Approx(3.0f));
    CHECK(player.IsComplete());
}

TEST_CASE("Update wraps time on a looping animation") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("run", MakeAnimation(atlas, 3.0f, true));
    player.Play("run");

    player.Update(4.5f);
    CHECK(player.GetTime() == doctest::Approx(1.5f));
    CHECK_FALSE(player.IsComplete());
}

TEST_CASE("Update clamps time on a non-looping animation") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("once", MakeAnimation(atlas, 2.0f, false));
    player.Play("once");

    player.Update(1.0f);
    CHECK(player.GetTime() == doctest::Approx(1.0f));
    CHECK_FALSE(player.IsComplete());

    player.Update(5.0f);
    CHECK(player.GetTime() == doctest::Approx(2.0f));
    CHECK(player.IsComplete());
}

TEST_CASE("Pause stops advancing time; Resume continues") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("run", MakeAnimation(atlas, 3.0f, true));
    player.Play("run");

    player.Update(0.5f);
    CHECK(player.GetTime() == doctest::Approx(0.5f));

    player.Pause();
    player.Update(1.0f); // should be ignored
    CHECK(player.GetTime() == doctest::Approx(0.5f));

    player.Resume();
    player.Update(0.25f);
    CHECK(player.GetTime() == doctest::Approx(0.75f));
}

TEST_CASE("Play resets paused state") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("run", MakeAnimation(atlas, 3.0f, true));
    player.Play("run");
    player.Update(0.5f);
    player.Pause();

    player.Play("run");
    player.Update(0.25f);
    CHECK(player.GetTime() == doctest::Approx(0.25f));
}

TEST_CASE("Looping animation never reports complete") {
    TextureAtlas atlas = MakeThreeFrameAtlas();
    SpriteAnimationPlayer player;
    player.AddAnimation("run", MakeAnimation(atlas, 1.0f, true));
    player.Play("run");

    player.Update(10.0f);
    CHECK_FALSE(player.IsComplete());
}
