#pragma once

#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include <SDL3/SDL_audio.h>

#include <Lucky/Audio/Sound.hpp>
#include <Lucky/Audio/Stream.hpp>

namespace Lucky
{
    typedef uint32_t AudioRef;

    enum class AudioState
    {
        Playing,
        Paused,
        Stopped,
    };

    struct SoundGroupSettings
    {
        SDL_AudioDeviceID deviceId = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
        float volume = 1.0f;
    };

    struct AudioDevice
    {
        std::string name;
        SDL_AudioDeviceID deviceId;
    };

    class AudioPlayer
    {
      public:
        static std::vector<AudioDevice> GetAudioOutputDevices();

        AudioPlayer(SoundGroupSettings defaultSoundGroupSettings = SoundGroupSettings());
        ~AudioPlayer();

        void CreateSoundGroup(const std::string &soundGroupName, SoundGroupSettings settings = SoundGroupSettings());
        void DestroySoundGroup(const std::string &soundGroupName);

        void StopGroup(const std::string &soundGroupName);
        void PauseGroup(const std::string &soundGroupName);
        void ResumeGroup(const std::string &soundGroupName);

        SDL_AudioDeviceID GetGroupDeviceId(const std::string &soundGroupName);
        float GetGroupVolume(const std::string &soundGroupName);

        AudioRef Play(
            std::shared_ptr<Sound> sound, const std::string &soundGroupName = "default", const bool loop = false);
        AudioRef Play(
            std::shared_ptr<Stream> stream, const std::string &soundGroupName = "default", const bool loop = false);
        void Pause(const AudioRef &audioRef);
        void Resume(const AudioRef &audioRef);
        void Stop(const AudioRef &audioRef);

        AudioState GetState(const AudioRef &audioRef);

        void Update();

      private:
        AudioPlayer(const AudioPlayer &) = delete;
        AudioPlayer(const AudioPlayer &&) = delete;
        AudioPlayer &operator=(const AudioPlayer &) = delete;
        AudioPlayer &operator=(const AudioPlayer &&) = delete;

        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };
} // namespace Lucky
