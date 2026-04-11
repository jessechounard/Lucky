#include <map>
#include <stdexcept>
#include <vector>

#include <Lucky/AudioPlayer.hpp>
#include <Lucky/Sound.hpp>
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace Lucky {

struct AudioInstance {
    enum class PutFramesResult {
        Success,
        ExhaustingQueue,
        Complete,
        Error,
    };

    AudioInstance(AudioPlayer &player, bool shouldLoop, AudioState state, AudioRef ref,
        const std::string &group, int16_t channels, int32_t sampleRate)
        : player(player), audioStream(nullptr), shouldLoop(shouldLoop), bound(false), state(state),
          ref(ref), group(group) {
        SDL_AudioSpec sourceSpec{SDL_AUDIO_S16, channels, sampleRate};

        audioStream = SDL_CreateAudioStream(&sourceSpec, &sourceSpec);
        if (!audioStream) {
            spdlog::error("Failed to create audio stream");
        }
    }

    virtual ~AudioInstance() {
        if (audioStream) {
            SDL_DestroyAudioStream(audioStream);
            audioStream = nullptr;
        }
    }

    void UpdateGain() {
        if (!audioStream) {
            return;
        }

        float groupVolume = player.GetGroupVolume(group);
        float gain = groupVolume * player.GetMasterVolume();

        if (!SDL_SetAudioStreamGain(audioStream, gain)) {
            spdlog::error("Failed to set audio stream gain: {}", SDL_GetError());
        }
    }

    virtual uint32_t GetSampleRate() const = 0;
    virtual PutFramesResult PutSamplesStream(uint32_t desiredFrameCount) = 0;

    AudioPlayer &player;
    SDL_AudioStream *audioStream;
    bool shouldLoop;
    bool bound;
    AudioState state;
    AudioRef ref;
    std::string group;
    std::vector<int16_t> workBuffer;
};

struct SoundInstance : public AudioInstance {
    SoundInstance(AudioPlayer &player, std::shared_ptr<Sound> sound, bool shouldLoop,
        AudioState state, AudioRef ref, const std::string &group)
        : AudioInstance(player, shouldLoop, state, ref, group, sound->channels, sound->sampleRate),
          sound(sound), position(0) {
    }

    ~SoundInstance() {
    }

    virtual uint32_t GetSampleRate() const {
        return sound->sampleRate;
    }

    virtual PutFramesResult PutSamplesStream(uint32_t desiredFrameCount) {
        if (!audioStream) {
            spdlog::error("Attempted to put samples into a null audio stream");
            return PutFramesResult::Error;
        }

        int bytesQueued = SDL_GetAudioStreamQueued(audioStream);
        uint32_t framesQueued = bytesQueued / sizeof(int16_t) / sound->channels;
        uint32_t framesNeeded = desiredFrameCount - framesQueued;

        if (framesQueued >= desiredFrameCount) {
            return PutFramesResult::Success;
        }

        size_t samplesNeeded = framesNeeded * sound->channels;
        if (workBuffer.size() < samplesNeeded) {
            workBuffer.resize(samplesNeeded);
        }
        bool didLoop = false;
        auto frameCount = sound->GetFrames(
            position, workBuffer.data(), framesNeeded, shouldLoop ? &didLoop : nullptr);

        if (frameCount == 0) {
            SDL_FlushAudioStream(audioStream);
            if (framesQueued == 0) {
                return PutFramesResult::Complete;
            }

            return PutFramesResult::ExhaustingQueue;
        }

        if (!SDL_PutAudioStreamData(
                audioStream, workBuffer.data(), frameCount * sound->channels * sizeof(int16_t))) {
            spdlog::error("Failed to put audio stream data: {}", SDL_GetError());
            return PutFramesResult::Error;
        }

        if (frameCount < framesNeeded) {
            return PutFramesResult::ExhaustingQueue;
        }
        return PutFramesResult::Success;
    }

    std::shared_ptr<Sound> sound;
    uint32_t position;
};

struct StreamInstance : public AudioInstance {
    StreamInstance(AudioPlayer &player, std::shared_ptr<Stream> stream, bool shouldLoop,
        AudioState state, AudioRef ref, const std::string &group)
        : AudioInstance(
              player, shouldLoop, state, ref, group, stream->channels, stream->sampleRate),
          stream(std::make_unique<Stream>(*stream)) {
    }

    ~StreamInstance() {
    }

    virtual uint32_t GetSampleRate() const {
        return stream->sampleRate;
    }

    virtual PutFramesResult PutSamplesStream(uint32_t desiredFrameCount) {
        if (!audioStream) {
            spdlog::error("Attempted to put samples into a null audio stream");
            return PutFramesResult::Error;
        }

        int bytesQueued = SDL_GetAudioStreamQueued(audioStream);
        uint32_t framesQueued = bytesQueued / sizeof(int16_t) / stream->channels;
        uint32_t framesNeeded = desiredFrameCount - framesQueued;

        if (framesQueued >= desiredFrameCount) {
            return PutFramesResult::Success;
        }

        size_t samplesNeeded = framesNeeded * stream->channels;
        if (workBuffer.size() < samplesNeeded) {
            workBuffer.resize(samplesNeeded);
        }
        bool didLoop = false;
        auto frameCount =
            stream->GetFrames(workBuffer.data(), framesNeeded, shouldLoop ? &didLoop : nullptr);

        if (frameCount == 0) {
            SDL_FlushAudioStream(audioStream);
            if (framesQueued == 0) {
                return PutFramesResult::Complete;
            }

            return PutFramesResult::ExhaustingQueue;
        }

        if (!SDL_PutAudioStreamData(
                audioStream, workBuffer.data(), frameCount * stream->channels * sizeof(int16_t))) {
            spdlog::error("Failed to put audio stream data: {}", SDL_GetError());
            return PutFramesResult::Error;
        }

        if (frameCount < framesNeeded) {
            return PutFramesResult::ExhaustingQueue;
        }
        return PutFramesResult::Success;
    }

    std::unique_ptr<Stream> stream;
};

struct SoundGroup {
    SDL_AudioDeviceID audioDeviceId = 0;
    SDL_AudioDeviceID logicalAudioDeviceId = 0;
    float volume = 0;
};

struct AudioPlayer::Impl {
    AudioRef nextAudioRef;
    SoundGroupSettings defaultSoundGroupSettings;

    std::vector<std::unique_ptr<AudioInstance>> instances;
    std::map<std::string, SoundGroup> soundGroups;

    float masterVolume;
    float queueTime;
};

std::vector<AudioDevice> AudioPlayer::GetAudioOutputDevices() {
    int count;
    SDL_AudioDeviceID *deviceIds = SDL_GetAudioPlaybackDevices(&count);
    std::vector<AudioDevice> audioDevices;

    for (int i = 0; i < count; i++) {
        const char *name = SDL_GetAudioDeviceName(deviceIds[i]);
        audioDevices.push_back(AudioDevice{name, deviceIds[i]});
    }

    SDL_free(deviceIds);
    return audioDevices;
}

AudioPlayer::AudioPlayer(SoundGroupSettings defaultSoundGroupSettings)
    : pImpl(std::make_unique<Impl>()) {
    pImpl->nextAudioRef = 1;
    pImpl->defaultSoundGroupSettings = defaultSoundGroupSettings;
    pImpl->masterVolume = 1.0f;
    pImpl->queueTime = 1 / 30.0f;

    CreateSoundGroup("default", defaultSoundGroupSettings);
}

AudioPlayer::~AudioPlayer() {
    pImpl->instances.clear();

    for (auto const &i : pImpl->soundGroups) {
        SDL_CloseAudioDevice(i.second.logicalAudioDeviceId);
    }
}

void AudioPlayer::CreateSoundGroup(const std::string &soundGroupName, SoundGroupSettings settings) {
    auto groupIterator = pImpl->soundGroups.find(soundGroupName);
    if (groupIterator != pImpl->soundGroups.end()) {
        SoundGroup &soundGroup = groupIterator->second;

        // if the device id is changing
        //    create the new one
        //    loop through all playing instances and rebind to the new one
        //    destroy the old one
        if (soundGroup.audioDeviceId != settings.deviceId) {
            SDL_AudioDeviceID newDeviceId = SDL_OpenAudioDevice(settings.deviceId, nullptr);
            if (newDeviceId == 0) {
                spdlog::error("Failed to open audio device with DeviceID {}: {}",
                    settings.deviceId,
                    SDL_GetError());
                throw std::runtime_error("Failed to open audio device");
            }

            for (auto const &i : pImpl->instances) {
                if (i->group == soundGroupName && i->state == AudioState::Playing) {
                    SDL_UnbindAudioStream(i->audioStream);
                    SDL_BindAudioStream(newDeviceId, i->audioStream);
                }
            }

            SDL_CloseAudioDevice(soundGroup.logicalAudioDeviceId);

            soundGroup.audioDeviceId = settings.deviceId;
            soundGroup.logicalAudioDeviceId = newDeviceId;
        }

        soundGroup.volume = settings.volume;

        for (auto const &i : pImpl->instances) {
            if (i->group == soundGroupName) {
                i->UpdateGain();
            }
        }
    } else {
        SoundGroup soundGroup;

        soundGroup.audioDeviceId = settings.deviceId;
        soundGroup.logicalAudioDeviceId = SDL_OpenAudioDevice(settings.deviceId, nullptr);
        if (soundGroup.logicalAudioDeviceId == 0) {
            spdlog::error("Failed to open audio device with DeviceID {}", settings.deviceId);
            throw std::runtime_error("Failed to open audio device");
        }
        soundGroup.volume = settings.volume;
        pImpl->soundGroups[soundGroupName] = soundGroup;
    }
}

void AudioPlayer::DestroySoundGroup(const std::string &soundGroupName) {
    StopGroup(soundGroupName);

    auto groupIterator = pImpl->soundGroups.find(soundGroupName);
    if (groupIterator != pImpl->soundGroups.end()) {
        SDL_CloseAudioDevice(groupIterator->second.logicalAudioDeviceId);
        pImpl->soundGroups.erase(groupIterator);
    }
}

void AudioPlayer::StopGroup(const std::string &soundGroupName) {
    pImpl->instances.erase(std::remove_if(pImpl->instances.begin(),
                               pImpl->instances.end(),
                               [soundGroupName](std::unique_ptr<AudioInstance> &ai) {
                                   return ai->group == soundGroupName;
                               }),
        pImpl->instances.end());
}

void AudioPlayer::PauseGroup(const std::string &soundGroupName) {
    for (auto const &i : pImpl->instances) {
        if (i->group == soundGroupName) {
            Pause(i->ref);
        }
    }
}

void AudioPlayer::ResumeGroup(const std::string &soundGroupName) {
    for (auto const &i : pImpl->instances) {
        if (i->group == soundGroupName) {
            Resume(i->ref);
        }
    }
}

SDL_AudioDeviceID AudioPlayer::GetGroupDeviceId(const std::string &soundGroupName) {
    auto iterator = pImpl->soundGroups.find(soundGroupName);
    if (iterator == pImpl->soundGroups.end()) {
        spdlog::error("Couldn't find audio group named {}", soundGroupName);
        throw std::runtime_error("Couldn't find audio group named " + soundGroupName);
    }

    return iterator->second.logicalAudioDeviceId;
}

float AudioPlayer::GetGroupVolume(const std::string &soundGroupName) {
    auto iterator = pImpl->soundGroups.find(soundGroupName);
    if (iterator == pImpl->soundGroups.end()) {
        spdlog::error("Couldn't find audio group named {}", soundGroupName);
        throw std::runtime_error("Couldn't find audio group named " + soundGroupName);
    }

    return iterator->second.volume;
}

float AudioPlayer::GetMasterVolume() const {
    return pImpl->masterVolume;
}

void AudioPlayer::SetMasterVolume(float volume) {
    pImpl->masterVolume = volume;

    for (auto const &i : pImpl->instances) {
        i->UpdateGain();
    }
}

AudioRef AudioPlayer::Play(
    std::shared_ptr<Sound> sound, const std::string &soundGroupName, const bool loop) {
    auto groupIterator = pImpl->soundGroups.find(soundGroupName);
    if (groupIterator == pImpl->soundGroups.end()) {
        CreateSoundGroup(soundGroupName, pImpl->defaultSoundGroupSettings);
    }

    AudioRef audioRef = pImpl->nextAudioRef++;

    pImpl->instances.push_back(std::make_unique<SoundInstance>(
        *this, sound, loop, AudioState::Playing, audioRef, soundGroupName));
    return audioRef;
}

AudioRef AudioPlayer::Play(
    std::shared_ptr<Stream> stream, const std::string &soundGroupName, const bool loop) {
    auto groupIterator = pImpl->soundGroups.find(soundGroupName);
    if (groupIterator == pImpl->soundGroups.end()) {
        CreateSoundGroup(soundGroupName, pImpl->defaultSoundGroupSettings);
    }

    AudioRef audioRef = pImpl->nextAudioRef++;

    pImpl->instances.push_back(std::make_unique<StreamInstance>(
        *this, stream, loop, AudioState::Playing, audioRef, soundGroupName));
    return audioRef;
}

void AudioPlayer::Pause(const AudioRef &audioRef) {
    auto iterator = std::find_if(pImpl->instances.begin(),
        pImpl->instances.end(),
        [audioRef](std::unique_ptr<AudioInstance> &ai) { return ai->ref == audioRef; });
    if (iterator == pImpl->instances.end() || (*iterator)->state == AudioState::Paused) {
        return;
    }

    (*iterator)->state = AudioState::Paused;
    (*iterator)->bound = false;
    SDL_UnbindAudioStream((*iterator)->audioStream);
}

void AudioPlayer::Resume(const AudioRef &audioRef) {
    auto iterator = std::find_if(pImpl->instances.begin(),
        pImpl->instances.end(),
        [audioRef](std::unique_ptr<AudioInstance> &ai) { return ai->ref == audioRef; });
    if (iterator == pImpl->instances.end() || (*iterator)->state == AudioState::Playing) {
        return;
    }

    (*iterator)->state = AudioState::Playing;
    (*iterator)->bound = true;
    if (!SDL_BindAudioStream(
            (*iterator)->player.GetGroupDeviceId((*iterator)->group), (*iterator)->audioStream)) {
        spdlog::error("Failed to bind audio stream: {}", SDL_GetError());
    }
}

void AudioPlayer::Stop(const AudioRef &audioRef) {
    auto iterator = std::find_if(pImpl->instances.begin(),
        pImpl->instances.end(),
        [audioRef](std::unique_ptr<AudioInstance> &ai) { return ai->ref == audioRef; });
    if (iterator == pImpl->instances.end()) {
        return;
    }

    pImpl->instances.erase(iterator);
}

AudioState AudioPlayer::GetState(const AudioRef &audioRef) {
    auto iterator = std::find_if(pImpl->instances.begin(),
        pImpl->instances.end(),
        [audioRef](std::unique_ptr<AudioInstance> &ai) { return ai->ref == audioRef; });
    if (iterator == pImpl->instances.end()) {
        return AudioState::Stopped;
    }

    return (*iterator)->state;
}

void AudioPlayer::Update() {
    for (auto &i : pImpl->instances) {
        if (i->state == AudioState::Playing) {
            uint32_t sampleRate = i->GetSampleRate();
            auto result = i->PutSamplesStream(static_cast<uint32_t>(pImpl->queueTime * sampleRate));
            if (result == AudioInstance::PutFramesResult::Complete) {
                i->state = AudioState::Stopped;
            } else if (!i->bound) {
                i->UpdateGain();
                if (!SDL_BindAudioStream(GetGroupDeviceId(i->group), i->audioStream)) {
                    spdlog::error("Failed to bind audio stream: {}", SDL_GetError());
                }
                i->bound = true;
            }
        }
    }

    pImpl->instances.erase(
        std::remove_if(pImpl->instances.begin(),
            pImpl->instances.end(),
            [](std::unique_ptr<AudioInstance> &ai) { return ai->state == AudioState::Stopped; }),
        pImpl->instances.end());
}

} // namespace Lucky
