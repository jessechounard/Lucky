#include <SDL3/SDL_filesystem.h>
#include <cstring>
#include <doctest/doctest.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Lucky/Stream.hpp>

using namespace Lucky;

namespace {
std::string FixturePath(const char *name) {
    return std::string(SDL_GetBasePath()) + "Fixtures/" + name;
}
} // namespace

TEST_CASE("Stream loads metadata from an Ogg Vorbis file") {
    Stream stream(FixturePath("sine.ogg"));
    CHECK(stream.sampleRate == 22050);
    CHECK(stream.channels == 1);
}

TEST_CASE("Stream loads metadata from an MP3 file") {
    Stream stream(FixturePath("sine.mp3"));
    CHECK(stream.sampleRate == 22050);
    CHECK(stream.channels == 1);
}

TEST_CASE("Stream throws when the file does not exist") {
    CHECK_THROWS_AS(Stream("Fixtures/does_not_exist.ogg"), std::runtime_error);
}

TEST_CASE("Stream::GetFrames advances the decoder") {
    Stream stream(FixturePath("sine.ogg"));

    std::vector<int16_t> buffer(100 * stream.channels);
    uint32_t got = stream.GetFrames(buffer.data(), 100, /*canLoop*/ false);
    CHECK(got == 100);

    uint32_t got2 = stream.GetFrames(buffer.data(), 100, /*canLoop*/ false);
    CHECK(got2 == 100);
    // Different point in the stream -- different bytes.
    // Just verify both calls returned data.
}

TEST_CASE("Stream::GetFrames stops at the end with canLoop=false") {
    Stream stream(FixturePath("sine.ogg"));

    // Pull way more than the 0.5s fixture has (~11k frames at 22050 Hz).
    std::vector<int16_t> buffer(100000 * stream.channels);
    bool didLoop = true;
    uint32_t got = stream.GetFrames(buffer.data(), 100000, /*canLoop*/ false, &didLoop);

    CHECK(got < 100000);
    CHECK(got > 0);
    CHECK(didLoop == false);
}

TEST_CASE("Stream::GetFrames wraps around with canLoop=true on Vorbis") {
    // Probe the stream length first, since Vorbis frame count is encoder-
    // dependent. Then ask for a count between one and two times that, so a
    // single wrap fills the request. (Stream::GetFrames handles at most one
    // wrap per call.)
    Stream probe(FixturePath("sine.ogg"));
    std::vector<int16_t> drainBuffer(100000 * probe.channels);
    uint32_t totalFrames = probe.GetFrames(drainBuffer.data(), 100000, /*canLoop*/ false);
    REQUIRE(totalFrames > 0);

    Stream stream(FixturePath("sine.ogg"));
    uint32_t requestCount = totalFrames + 100;
    std::vector<int16_t> buffer(requestCount * stream.channels);
    bool didLoop = false;
    uint32_t got = stream.GetFrames(buffer.data(), requestCount, /*canLoop*/ true, &didLoop);

    CHECK(got == requestCount);
    CHECK(didLoop == true);
}

TEST_CASE("Stream::GetFrames wraps around with canLoop=true on MP3") {
    Stream probe(FixturePath("sine.mp3"));
    std::vector<int16_t> drainBuffer(100000 * probe.channels);
    uint32_t totalFrames = probe.GetFrames(drainBuffer.data(), 100000, /*canLoop*/ false);
    REQUIRE(totalFrames > 0);

    Stream stream(FixturePath("sine.mp3"));
    uint32_t requestCount = totalFrames + 100;
    std::vector<int16_t> buffer(requestCount * stream.channels);
    bool didLoop = false;
    uint32_t got = stream.GetFrames(buffer.data(), requestCount, /*canLoop*/ true, &didLoop);

    CHECK(got == requestCount);
    CHECK(didLoop == true);
}

TEST_CASE("Stream::GetFrames clears didLoop when no wrap happens") {
    Stream stream(FixturePath("sine.ogg"));

    std::vector<int16_t> buffer(100 * stream.channels);
    bool didLoop = true;
    stream.GetFrames(buffer.data(), 100, /*canLoop*/ true, &didLoop);

    CHECK(didLoop == false);
}

TEST_CASE("Stream::Clone produces an independent decoder cursor") {
    Stream original(FixturePath("sine.ogg"));

    // Advance the original past some frames.
    std::vector<int16_t> scratch(500 * original.channels);
    original.GetFrames(scratch.data(), 500, false);

    // Clone after advancing; clone starts at 0.
    Stream clone = original.Clone();

    std::vector<int16_t> originalBuffer(100 * original.channels);
    std::vector<int16_t> cloneBuffer(100 * clone.channels);

    original.GetFrames(originalBuffer.data(), 100, false);
    clone.GetFrames(cloneBuffer.data(), 100, false);

    // Different positions in the stream → different data.
    CHECK(memcmp(originalBuffer.data(),
              cloneBuffer.data(),
              originalBuffer.size() * sizeof(int16_t)) != 0);
}

TEST_CASE("Stream is move-constructible") {
    Stream s1(FixturePath("sine.ogg"));
    Stream s2 = std::move(s1);

    // s2 should be functional after the move.
    std::vector<int16_t> buffer(100 * s2.channels);
    uint32_t got = s2.GetFrames(buffer.data(), 100, false);
    CHECK(got == 100);

    // s1's destructor must not crash with the moved-from null pImpl.
}
