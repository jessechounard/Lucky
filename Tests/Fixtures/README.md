# Test Fixtures

Small audio files used by Sound and Stream tests to exercise their decoder
paths. All files are tone generators with no copyright concerns.

## Files

- `sine.wav` — 0.5s, 22050 Hz mono, 16-bit PCM, 440 Hz sine wave.
- `sine.ogg` — same content as Ogg Vorbis.
- `sine.mp3` — same content as MP3.

## Regenerating

If a future format change or decoder upgrade requires regenerating these:

    ffmpeg -y -f lavfi -i "sine=frequency=440:duration=0.5:sample_rate=22050" -c:a pcm_s16le sine.wav
    ffmpeg -y -f lavfi -i "sine=frequency=440:duration=0.5:sample_rate=22050" sine.ogg
    ffmpeg -y -f lavfi -i "sine=frequency=440:duration=0.5:sample_rate=22050" sine.mp3

The exact byte content does not matter for tests -- they only verify
metadata (sample rate, channels) and decoder behavior (cursor advance,
loop wrap, Clone independence). Pick any duration / frequency / channel
count if you need different test conditions.
