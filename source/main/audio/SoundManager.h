/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2020 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_OPENAL

#pragma once

#include "Application.h"
#include "Collisions.h"
#include "GameContext.h"
#include "Sound.h"

#include <OgreVector3.h>
#include <OgreString.h>

#ifdef __APPLE__
  #include <OpenAL/al.h>
  #include <OpenAL/alc.h>
  #include <OpenAL/alext.h>
  #include <OpenAL/efx-presets.h>
#else
  #include <AL/al.h>
  #include <AL/alc.h>
  #include <AL/alext.h>
  #include <AL/efx-presets.h>
#endif // __APPLE__

namespace RoR {

/// @addtogroup Audio
/// @{

class SoundManager
{
    friend class Sound;

public:
    SoundManager();
    ~SoundManager();

    /**
    * @param filename WAV file.
    * @param resource_group_name Leave empty to auto-search all groups (classic behavior).
    */
    SoundPtr createSound(Ogre::String filename, Ogre::String resource_group_name = "");

    void setListener(Ogre::Vector3 position, Ogre::Vector3 direction, Ogre::Vector3 up, Ogre::Vector3 velocity);
    void setListenerEnvironment(std::string listener_environment);
    void pauseAllSounds();
    void resumeAllSounds();
    void setMasterVolume(float v);

    bool isDisabled() { return audio_device == 0; }

    /**
    * Returns the speed of sound that is currently set for OpenAL
    * @return Speed of Sound
    */
    float getSpeedOfSound() { return alGetFloat(AL_SPEED_OF_SOUND); }

    /**
    * Sets the speed of sound
    * @param speed_of_sound Speed of Sound in RoR_unit (=1 meter)/ second (must not be negative)
    */
    void setSpeedOfSound(float speed_of_sound) { alSpeedOfSound(speed_of_sound); }

    /**
    * Returns the value set for the Doppler Factor
    * @return Doppler Factor
    */
    float getDopplerFactor() { return alGetFloat(AL_DOPPLER_FACTOR); }

    /**
    * Sets the Doppler Factor
    * @param doppler_factor Doppler Factor (must not be negative)
    */
    void setDopplerFactor(float doppler_factor) { alDopplerFactor(doppler_factor); }

    /**
    * Registers an OpenAL EFX Preset
    * @param preset_name Name of the EFX Preset (should later be used to reference it)
    * @param efx_properties EFXEAXREVERBPROPERTIES structure that stores the properties of the EFX preset
    * @return Returns true if the preset was successfully inserted, false otherwise
    * @see https://github.com/kcat/openal-soft/blob/36f2a1c31261a1f97162b90d8f1e80c68d059183/include/AL/efx-presets.h#L9-L33
    * @see https://openal-soft.org/misc-downloads/Effects%20Extension%20Guide.pdf
    */
    bool registerEfxPreset(std::string preset_name, EFXEAXREVERBPROPERTIES efx_properties);

    /**
    * Unregisters an OpenAL EFX Preset by name
    * @param preset_name Name of the EFX Preset that should be unregistered. This deletes the preset and renders it unusable.
    * @return Returns true if the preset was found and deleted, false otherwise
    */
    bool unregisterEfxPreset(std::string preset_name);

    int getNumHardwareSources() { return hardware_sources_num; }

    static const float MAX_DISTANCE;
    static const float ROLLOFF_FACTOR;
    static const float REFERENCE_DISTANCE;
    static const unsigned int MAX_HARDWARE_SOURCES = 32;
    static const unsigned int MAX_AUDIO_BUFFERS = 8192;

private:
    void recomputeAllSources();
    void recomputeSource(int source_index, int reason, float vfl, Ogre::Vector3 *vvec);
    ALuint getHardwareSource(int hardware_index) { return hardware_sources[hardware_index]; };

    void assign(int source_index, int hardware_index);
    void retire(int source_index);

    bool loadWAVFile(Ogre::String filename, ALuint buffer, Ogre::String resource_group_name = "");

    // active audio sources (hardware sources)
    int    hardware_sources_num = 0;                       // total number of available hardware sources < MAX_HARDWARE_SOURCES
    int    hardware_sources_in_use_count = 0;
    int    hardware_sources_map[MAX_HARDWARE_SOURCES]; // stores the hardware index for each source. -1 = unmapped
    ALuint hardware_sources[MAX_HARDWARE_SOURCES];     // this buffer contains valid AL handles up to m_hardware_sources_num

    // audio sources
    SoundPtr audio_sources[MAX_AUDIO_BUFFERS] = { nullptr };
    // helper for calculating the most audible sources
    std::pair<int, float> audio_sources_most_audible[MAX_AUDIO_BUFFERS];
    
    // audio buffers: Array of AL buffers and filenames
    int          audio_buffers_in_use_count = 0;
    ALuint       audio_buffers[MAX_AUDIO_BUFFERS];
    Ogre::String audio_buffer_file_name[MAX_AUDIO_BUFFERS];

    Ogre::Vector3 listener_position = Ogre::Vector3::ZERO;
    ALCdevice*    audio_device = nullptr;
    ALCcontext*   sound_context = nullptr;

    // OpenAL EFX stuff
    bool                                            efx_is_available = false;
    bool                                            listener_efx_environment_has_changed = true;
    ALuint                                          listener_slot = 0;
    ALuint                                          efx_outdoor_obstruction_lowpass_filter_id = 0;

    enum EfxReverbEngine
    {
        NONE,
        REVERB,
        EAXREVERB
    };
    EfxReverbEngine                                 efx_reverb_engine = EfxReverbEngine::NONE;

    std::string                                     listener_efx_preset_name;
    std::map<std::string, EFXEAXREVERBPROPERTIES>   efx_properties_map;
    std::map<std::string, ALuint>                   efx_effect_id_map;
    std::map<std::string, EfxReverbEngine>          efx_reverb_engine_map =
                                                        {{"EAXREVERB", EfxReverbEngine::EAXREVERB},
                                                        {"REVERB", EfxReverbEngine::REVERB},
                                                        {"NONE", EfxReverbEngine::NONE}};
    LPALGENEFFECTS                                  alGenEffects = nullptr;
    LPALDELETEEFFECTS                               alDeleteEffects = nullptr;
    LPALISEFFECT                                    alIsEffect = nullptr;
    LPALEFFECTI                                     alEffecti = nullptr;
    LPALEFFECTF                                     alEffectf = nullptr;
    LPALEFFECTFV                                    alEffectfv = nullptr;
    LPALGENFILTERS                                  alGenFilters = nullptr;
    LPALDELETEFILTERS                               alDeleteFilters = nullptr;
    LPALISFILTER                                    alIsFilter = nullptr;
    LPALFILTERI                                     alFilteri = nullptr;
    LPALFILTERF                                     alFilterf = nullptr;
    LPALGENAUXILIARYEFFECTSLOTS                     alGenAuxiliaryEffectSlots = nullptr;
    LPALDELETEAUXILIARYEFFECTSLOTS                  alDeleteAuxiliaryEffectSlots = nullptr;
    LPALISAUXILIARYEFFECTSLOT                       alIsAuxiliaryEffectSlot = nullptr;
    LPALAUXILIARYEFFECTSLOTI                        alAuxiliaryEffectSloti = nullptr;
    LPALAUXILIARYEFFECTSLOTF                        alAuxiliaryEffectSlotf = nullptr;
    LPALAUXILIARYEFFECTSLOTFV                       alAuxiliaryEffectSlotfv = nullptr;

    ALuint  CreateAlEffect(const EFXEAXREVERBPROPERTIES* efx_properties);
    void    prepopulate_efx_property_map();
    void    updateListenerEffectSlot();
};

/// @}

} // namespace RoR

#endif // USE_OPENAL
