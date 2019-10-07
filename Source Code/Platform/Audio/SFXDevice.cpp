#include "stdafx.h"

#include "Headers/SFXDevice.h"

#include "Platform/Audio/fmod/Headers/FmodWrapper.h"
#include "Platform/Audio/sdl_mixer/Headers/SDLWrapper.h"
#include "Platform/Audio/openAl/Headers/ALWrapper.h"

#include "Core/Headers/Console.h"
#include "Utility/Headers/Localization.h"

namespace Divide {

SFXDevice::SFXDevice(Kernel& parent)
    : KernelComponent(parent), 
      AudioAPIWrapper(),
      _state(true, true, true, true),
      _API_ID(AudioAPI::COUNT),
      _api(nullptr)
{
    _playNextInPlaylist = false;
}

SFXDevice::~SFXDevice()
{
    closeAudioAPI();
}

ErrorCode SFXDevice::initAudioAPI(PlatformContext& context) {
    assert(_api == nullptr && "SFXDevice error: initAudioAPI called twice!");

    switch (_API_ID) {
        case AudioAPI::FMOD: {
            _api = std::make_unique<FMOD_API>();
        } break;
        case AudioAPI::OpenAL: {
            _api = std::make_unique<OpenAL_API>();
        } break;
        case AudioAPI::SDL: {
            _api = std::make_unique<SDL_API>();
        } break;
        default: {
            Console::errorfn(Locale::get(_ID("ERROR_SFX_DEVICE_API")));
            return ErrorCode::SFX_NON_SPECIFIED;
        } break;
    };

    return _api->initAudioAPI(context);
}

void SFXDevice::closeAudioAPI() {
    _musicPlaylists.clear();
    _currentPlaylist.second.clear();

    if (_api != nullptr) {
        _api->closeAudioAPI();
        _api.reset();
    }
}

void SFXDevice::beginFrame() {
    _api->beginFrame();

    if (_playNextInPlaylist) {
        _api->musicFinished();

        if (!_currentPlaylist.second.empty()) {
            _currentPlaylist.first = ++_currentPlaylist.first % _currentPlaylist.second.size();
            _api->playMusic(_currentPlaylist.second[_currentPlaylist.first]);
        }
        _playNextInPlaylist = false;
    }
}

void SFXDevice::endFrame() {
    _api->endFrame();
}

void SFXDevice::playSound(const AudioDescriptor_ptr& sound) {
    DIVIDE_ASSERT(_api != nullptr,
                "SFXDevice error: playSound called without init!");

    _api->playSound(sound);
}

void SFXDevice::addMusic(U32 playlistEntry, const AudioDescriptor_ptr& music) {
    MusicPlaylist& playlist = _musicPlaylists[playlistEntry];
    playlist.second.push_back(music);
    playlist.first = 0;
}

bool SFXDevice::playMusic(U32 playlistEntry) {
    MusicPlaylists::iterator it = _musicPlaylists.find(playlistEntry);
    if (it != std::cend(_musicPlaylists)) {
        return playMusic(it->second);
    }

    return false;
}

bool SFXDevice::playMusic(MusicPlaylist& playlist) {
    if (!playlist.second.empty()) {
        _currentPlaylist = playlist;
        _api->playMusic(_currentPlaylist.second[_currentPlaylist.first]);
        return true;
    }

    return false;
}

void SFXDevice::playMusic(const AudioDescriptor_ptr& music) {
    _api->playMusic(music);
}

void SFXDevice::pauseMusic() {
    DIVIDE_ASSERT(_api != nullptr,
                "SFXDevice error: pauseMusic called without init!");

    _api->pauseMusic();
}

void SFXDevice::stopMusic() {
    DIVIDE_ASSERT(_api != nullptr,
                "SFXDevice error: stopMusic called without init!");

    _api->stopMusic();
}

void SFXDevice::stopAllSounds() {
    DIVIDE_ASSERT(_api != nullptr,
                "SFXDevice error: stopAllSounds called without init!");

    _api->stopAllSounds();
}

void SFXDevice::setMusicVolume(I8 value) {
    DIVIDE_ASSERT(_api != nullptr,
                "SFXDevice error: setMusicVolume called without init!");

    _api->setMusicVolume(value);
}

void SFXDevice::setSoundVolume(I8 value) {
    DIVIDE_ASSERT(_api != nullptr,
                "SFXDevice error: setSoundVolume called without init!");

    _api->setSoundVolume(value);
}

void SFXDevice::musicFinished() {
    _playNextInPlaylist = true;
}

void SFXDevice::dumpPlaylists() {
    _currentPlaylist = MusicPlaylist();
    _musicPlaylists.clear();
}

}; //namespace Divide
