#include <SDL3/SDL_filesystem.h>
#include <cstring>
#include <doctest/doctest.h>
#include <stdexcept>
#include <string>
#include <vector>

#include <Lucky/Sound.hpp>

using namespace Lucky;

namespace {
std::string FixturePath(const char *name) {
    return std::string(SDL_GetBasePath()) + "Fixtures/" + name;
}
} // namespace

TEST_CASE("Sound loads metadata from a WAV file") {
    Sound sound(FixturePath("sine.wav"));
    CHECK(sound.sampleRate == 22050);
    CHECK(sound.channels == 1);
    CHECK(sound.totalFrames > 0);
    CHECK(sound.frames.size() == sound.totalFrames * sound.channels);
}

TEST_CASE("Sound throws when the file does not exist") {
    CHECK_THROWS_AS(Sound("Fixtures/does_not_exist.wav"), std::runtime_error);
}

TEST_CASE("Sound::GetFrames advances the caller's cursor") {
    Sound sound(FixturePath("sine.wav"));

    std::vector<int16_t> buffer(100 * sound.channels);
    uint32_t position = 0;
    uint32_t got = sound.GetFrames(position, buffer.data(), 100, /*canLoop*/ false);

    CHECK(got == 100);
    CHECK(position == 100);
}

TEST_CASE("Sound::GetFrames stops at the end with canLoop=false") {
    Sound sound(FixturePath("sine.wav"));

    std::vector<int16_t> buffer(sound.totalFrames * sound.channels * 2);
    uint32_t position = sound.totalFrames - 50;

    bool didLoop = true; // pre-set to ensure it gets cleared
    uint32_t got = sound.GetFrames(position, buffer.data(), 200, /*canLoop*/ false, &didLoop);

    CHECK(got == 50);
    CHECK(position == sound.totalFrames);
    CHECK(didLoop == false);
}

TEST_CASE("Sound::GetFrames wraps around with canLoop=true") {
    Sound sound(FixturePath("sine.wav"));

    std::vector<int16_t> buffer(200 * sound.channels);
    uint32_t position = sound.totalFrames - 50;

    bool didLoop = false;
    uint32_t got = sound.GetFrames(position, buffer.data(), 200, /*canLoop*/ true, &didLoop);

    CHECK(got == 200);
    CHECK(position == 150); // wrapped to 0, then read 150 more
    CHECK(didLoop == true);
}

TEST_CASE("Sound::GetFrames clears didLoop when no wrap happens") {
    Sound sound(FixturePath("sine.wav"));

    std::vector<int16_t> buffer(50 * sound.channels);
    uint32_t position = 0;

    bool didLoop = true; // pre-set to ensure it gets cleared
    sound.GetFrames(position, buffer.data(), 50, /*canLoop*/ true, &didLoop);

    CHECK(didLoop == false);
}

TEST_CASE("Sound::GetFrames accepts a null didLoop pointer") {
    Sound sound(FixturePath("sine.wav"));

    std::vector<int16_t> buffer(100 * sound.channels);
    uint32_t position = sound.totalFrames - 25;
    uint32_t got = sound.GetFrames(position, buffer.data(), 100, /*canLoop*/ true, nullptr);

    CHECK(got == 100);
    CHECK(position == 75);
}

TEST_CASE("Sound is shareable across independent cursors") {
    Sound sound(FixturePath("sine.wav"));

    std::vector<int16_t> bufferA(50 * sound.channels);
    std::vector<int16_t> bufferB(50 * sound.channels);
    uint32_t cursorA = 0;
    uint32_t cursorB = 0;

    sound.GetFrames(cursorA, bufferA.data(), 50, false);
    sound.GetFrames(cursorB, bufferB.data(), 50, false);

    CHECK(cursorA == 50);
    CHECK(cursorB == 50);
    // Same starting point → same data.
    CHECK(memcmp(bufferA.data(), bufferB.data(), bufferA.size() * sizeof(int16_t)) == 0);
}
