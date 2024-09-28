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

#include "SoundManager.h"

#include "Application.h"
#include "Sound.h"

#include <OgreResourceGroupManager.h>

#define LOGSTREAM Ogre::LogManager::getSingleton().stream() << "[RoR|Audio] "

bool _checkALErrors(const char* filename, int linenum)
{
    int err = alGetError();
    if (err != AL_NO_ERROR)
    {
        char buf[1000] = {};
        snprintf(buf, 1000, "OpenAL Error: %s (0x%x), @ %s:%d", alGetString(err), err, filename, linenum);
        LOGSTREAM << buf;
        return true;
    }
    return false;
}

#define hasALErrors() _checkALErrors(__FILE__, __LINE__)

using namespace RoR;
using namespace Ogre;

const float SoundManager::MAX_DISTANCE = 500.0f;
const float SoundManager::ROLLOFF_FACTOR = 1.0f;
const float SoundManager::REFERENCE_DISTANCE = 7.5f;

SoundManager::SoundManager()
{
    if (App::audio_device_name->getStr() == "")
    {
        LOGSTREAM << "No audio device configured, opening default.";
        audio_device = alcOpenDevice(nullptr);
    }
    else
    {
        audio_device = alcOpenDevice(App::audio_device_name->getStr().c_str());
        if (!audio_device)
        {
            LOGSTREAM << "Failed to open configured audio device \"" << App::audio_device_name->getStr() << "\", opening default.";
            App::audio_device_name->setStr("");
            audio_device = alcOpenDevice(nullptr);
        }
    }

    if (!audio_device)
    {
        LOGSTREAM << "Failed to open default audio device. Sound disabled.";
        hasALErrors();
        return;
    }

    sound_context = alcCreateContext(audio_device, NULL);

    if (!sound_context)
    {
        alcCloseDevice(audio_device);
        audio_device = NULL;
        hasALErrors();
        return;
    }

    alcMakeContextCurrent(sound_context);

    if (alGetString(AL_VENDOR)) LOG("SoundManager: OpenAL vendor is: " + String(alGetString(AL_VENDOR)));
    if (alGetString(AL_VERSION)) LOG("SoundManager: OpenAL version is: " + String(alGetString(AL_VERSION)));
    if (alGetString(AL_RENDERER)) LOG("SoundManager: OpenAL renderer is: " + String(alGetString(AL_RENDERER)));
    if (alGetString(AL_EXTENSIONS)) LOG("SoundManager: OpenAL extensions are: " + String(alGetString(AL_EXTENSIONS)));
    if (alcGetString(audio_device, ALC_DEVICE_SPECIFIER)) LOG("SoundManager: OpenAL device is: " + String(alcGetString(audio_device, ALC_DEVICE_SPECIFIER)));
    if (alcGetString(audio_device, ALC_EXTENSIONS)) LOG("SoundManager: OpenAL ALC extensions are: " + String(alcGetString(audio_device, ALC_EXTENSIONS)));

    // initialize use of OpenAL EFX extensions
    this->efx_is_available = alcIsExtensionPresent(audio_device, "ALC_EXT_EFX");
    if (efx_is_available)
    {
        LOG("SoundManager: Found OpenAL EFX extension");

        // Get OpenAL function pointers
        alGenEffects = (LPALGENEFFECTS)alGetProcAddress("alGenEffects");
        alDeleteEffects = (LPALDELETEEFFECTS)alGetProcAddress("alDeleteEffects");
        alIsEffect = (LPALISEFFECT)alGetProcAddress("alIsEffect");
        alEffecti = (LPALEFFECTI)alGetProcAddress("alEffecti");
        alEffectf = (LPALEFFECTF)alGetProcAddress("alEffectf");
        alEffectfv = (LPALEFFECTFV)alGetProcAddress("alEffectfv");
        alGenFilters = (LPALGENFILTERS)alGetProcAddress("alGenFilters");
        alDeleteFilters = (LPALDELETEFILTERS)alGetProcAddress("alDeleteFilters");
        alIsFilter = (LPALISFILTER)alGetProcAddress("alIsFilter");
        alFilteri = (LPALFILTERI)alGetProcAddress("alFilteri");
        alFilterf = (LPALFILTERF)alGetProcAddress("alFilterf");
        alGenAuxiliaryEffectSlots = (LPALGENAUXILIARYEFFECTSLOTS)alGetProcAddress("alGenAuxiliaryEffectSlots");
        alDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress("alDeleteAuxiliaryEffectSlots");
        alIsAuxiliaryEffectSlot = (LPALISAUXILIARYEFFECTSLOT)alGetProcAddress("alIsAuxiliaryEffectSlot");
        alAuxiliaryEffectSloti = (LPALAUXILIARYEFFECTSLOTI)alGetProcAddress("alAuxiliaryEffectSloti");
        alAuxiliaryEffectSlotf = (LPALAUXILIARYEFFECTSLOTF)alGetProcAddress("alAuxiliaryEffectSlotf");
        alAuxiliaryEffectSlotfv = (LPALAUXILIARYEFFECTSLOTFV)alGetProcAddress("alAuxiliaryEffectSlotfv");

        if (App::audio_enable_efx->getBool())
        {
            // allow user to change reverb engines at will
            switch(App::audio_efx_reverb_engine->getEnum<EfxReverbEngine>())
            {
                case EfxReverbEngine::EAXREVERB: efx_reverb_engine = EfxReverbEngine::EAXREVERB; break;
                case EfxReverbEngine::REVERB:    efx_reverb_engine = EfxReverbEngine::REVERB; break;
                default:
                    efx_reverb_engine = EfxReverbEngine::NONE;
                    LOG("SoundManager: Reverb engine disabled");
            }

            if(efx_reverb_engine == EfxReverbEngine::EAXREVERB)
            {
                if (alGetEnumValue("AL_EFFECT_EAXREVERB") != 0)
                {
                    LOG("SoundManager: OpenAL driver supports AL_EFFECT_EAXREVERB, using it");
                }
                else
                {
                    LOG("SoundManager: AL_EFFECT_EAXREVERB requested but OpenAL driver does not support it, falling back to standard reverb. Advanced features, such as reflection panning, will not be available");
                    efx_reverb_engine = EfxReverbEngine::REVERB;
                }
            }
            else if(efx_reverb_engine == EfxReverbEngine::REVERB)
            {
                LOG("SoundManager: Using OpenAL standard reverb");
            }

            // create effect slot for the listener
            if(!this->alIsAuxiliaryEffectSlot(listener_slot))
            {
                alGetError();

                this->alGenAuxiliaryEffectSlots(1, &listener_slot);
                ALuint e = alGetError();

                if (e != AL_NO_ERROR)
                {
                    LOG("SoundManager: alGenAuxiliaryEffectSlots for listener_slot failed: " + e);
                    listener_slot = AL_EFFECTSLOT_NULL;
                }
            }

            this->prepopulate_efx_property_map();

            /*
                Create filter for obstruction
                Currently we don't check for how much high-frequency content the obstacle
                lets through. We assume it's a hard surface with significant absorption
                of high frequencies (which should be true for trucks, buildings and terrain).
            */
            alGetError();

            alGenFilters(1, &efx_outdoor_obstruction_lowpass_filter_id);
            ALuint e = alGetError();

            if (e != AL_NO_ERROR)
            {
                efx_outdoor_obstruction_lowpass_filter_id = AL_FILTER_NULL;
            }
            else
            {
                alFilteri(efx_outdoor_obstruction_lowpass_filter_id, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
                alFilterf(efx_outdoor_obstruction_lowpass_filter_id, AL_LOWPASS_GAIN, 0.33f);
                alFilterf(efx_outdoor_obstruction_lowpass_filter_id, AL_LOWPASS_GAINHF, 0.25f);
            }
        }
    }
    else
    {
        LOG("SoundManager: OpenAL EFX extension not found, disabling EFX");
        App::audio_enable_efx->setVal(false);
    }

    // generate the AL sources
    for (hardware_sources_num = 0; hardware_sources_num < MAX_HARDWARE_SOURCES; hardware_sources_num++)
    {
        alGetError();
        alGenSources(1, &hardware_sources[hardware_sources_num]);
        if (alGetError() != AL_NO_ERROR)
            break;
        alSourcef(hardware_sources[hardware_sources_num], AL_REFERENCE_DISTANCE, REFERENCE_DISTANCE);
        alSourcef(hardware_sources[hardware_sources_num], AL_ROLLOFF_FACTOR, ROLLOFF_FACTOR);
        alSourcef(hardware_sources[hardware_sources_num], AL_MAX_DISTANCE, MAX_DISTANCE);

        // connect source to listener slot effect
        if(App::audio_enable_efx->getBool())
        {
            alSource3i(hardware_sources[hardware_sources_num], AL_AUXILIARY_SEND_FILTER, listener_slot, 0, AL_FILTER_NULL);
        }
    }

    alDopplerFactor(App::audio_doppler_factor->getFloat());
    alSpeedOfSound(343.3f);

    for (int i = 0; i < MAX_HARDWARE_SOURCES; i++)
    {
        hardware_sources_map[i] = -1;
    }


}

SoundManager::~SoundManager()
{
    // delete the sources and buffers
    alDeleteSources(MAX_HARDWARE_SOURCES, hardware_sources);
    alDeleteBuffers(MAX_AUDIO_BUFFERS, audio_buffers);

    if(efx_is_available)
    {
        if(alIsFilter(efx_outdoor_obstruction_lowpass_filter_id))
        {
            alDeleteFilters(1, &efx_outdoor_obstruction_lowpass_filter_id);
        }

        for (auto const& efx_effect_id : efx_effect_id_map)
        {
            alDeleteEffects(1, &efx_effect_id.second);
        }

        if (alIsAuxiliaryEffectSlot(listener_slot))
        {
            alAuxiliaryEffectSloti(listener_slot, AL_EFFECTSLOT_EFFECT, AL_EFFECTSLOT_NULL);
            alDeleteAuxiliaryEffectSlots(1, &listener_slot);
            listener_slot = AL_EFFECTSLOT_NULL;
        }
    }

    // destroy the sound context and device
    sound_context = alcGetCurrentContext();
    audio_device = alcGetContextsDevice(sound_context);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(sound_context);
    if (audio_device)
    {
        alcCloseDevice(audio_device);
    }
    LOG("SoundManager destroyed.");
}

void SoundManager::prepopulate_efx_property_map()
{
    this->efx_properties_map["EFX_REVERB_PRESET_GENERIC"] = EFX_REVERB_PRESET_GENERIC;
    this->efx_properties_map["EFX_REVERB_PRESET_CAVE"] = EFX_REVERB_PRESET_CAVE;
    this->efx_properties_map["EFX_REVERB_PRESET_ARENA"] = EFX_REVERB_PRESET_ARENA;
    this->efx_properties_map["EFX_REVERB_PRESET_HANGAR"] = EFX_REVERB_PRESET_HANGAR;
    this->efx_properties_map["EFX_REVERB_PRESET_ALLEY"] = EFX_REVERB_PRESET_ALLEY;
    this->efx_properties_map["EFX_REVERB_PRESET_FOREST"] = EFX_REVERB_PRESET_FOREST;
    this->efx_properties_map["EFX_REVERB_PRESET_CITY"] = EFX_REVERB_PRESET_CITY;
    this->efx_properties_map["EFX_REVERB_PRESET_MOUNTAINS"] = EFX_REVERB_PRESET_MOUNTAINS;
    this->efx_properties_map["EFX_REVERB_PRESET_QUARRY"] = EFX_REVERB_PRESET_QUARRY;
    this->efx_properties_map["EFX_REVERB_PRESET_PLAIN"] = EFX_REVERB_PRESET_PLAIN;
    this->efx_properties_map["EFX_REVERB_PRESET_PARKINGLOT"] = EFX_REVERB_PRESET_PARKINGLOT;
    this->efx_properties_map["EFX_REVERB_PRESET_UNDERWATER"] = EFX_REVERB_PRESET_UNDERWATER;
    this->efx_properties_map["EFX_REVERB_PRESET_DRUGGED"] = EFX_REVERB_PRESET_DRUGGED;
    this->efx_properties_map["EFX_REVERB_PRESET_DIZZY"] = EFX_REVERB_PRESET_DIZZY;
    this->efx_properties_map["EFX_REVERB_PRESET_CASTLE_COURTYARD"] = EFX_REVERB_PRESET_CASTLE_COURTYARD;
    this->efx_properties_map["EFX_REVERB_PRESET_FACTORY_HALL"] = EFX_REVERB_PRESET_FACTORY_HALL;
    this->efx_properties_map["EFX_REVERB_PRESET_SPORT_EMPTYSTADIUM"] = EFX_REVERB_PRESET_SPORT_EMPTYSTADIUM;
    this->efx_properties_map["EFX_REVERB_PRESET_PREFAB_WORKSHOP"] = EFX_REVERB_PRESET_PREFAB_WORKSHOP;
    this->efx_properties_map["EFX_REVERB_PRESET_PREFAB_CARAVAN"] = EFX_REVERB_PRESET_PREFAB_CARAVAN;
    this->efx_properties_map["EFX_REVERB_PRESET_PIPE_LARGE"] = EFX_REVERB_PRESET_PIPE_LARGE;
    this->efx_properties_map["EFX_REVERB_PRESET_PIPE_LONGTHIN"] = EFX_REVERB_PRESET_PIPE_LONGTHIN;
    this->efx_properties_map["EFX_REVERB_PRESET_PIPE_RESONANT"] = EFX_REVERB_PRESET_PIPE_RESONANT;
    this->efx_properties_map["EFX_REVERB_PRESET_OUTDOORS_BACKYARD"] = EFX_REVERB_PRESET_OUTDOORS_BACKYARD;
    this->efx_properties_map["EFX_REVERB_PRESET_OUTDOORS_ROLLINGPLAINS"] = EFX_REVERB_PRESET_OUTDOORS_ROLLINGPLAINS;
    this->efx_properties_map["EFX_REVERB_PRESET_OUTDOORS_DEEPCANYON"] = EFX_REVERB_PRESET_OUTDOORS_DEEPCANYON;
    this->efx_properties_map["EFX_REVERB_PRESET_OUTDOORS_CREEK"] = EFX_REVERB_PRESET_OUTDOORS_CREEK;
    this->efx_properties_map["EFX_REVERB_PRESET_OUTDOORS_VALLEY"] = EFX_REVERB_PRESET_OUTDOORS_VALLEY;
    this->efx_properties_map["EFX_REVERB_PRESET_MOOD_HEAVEN"] = EFX_REVERB_PRESET_MOOD_HEAVEN;
    this->efx_properties_map["EFX_REVERB_PRESET_MOOD_HELL"] = EFX_REVERB_PRESET_MOOD_HELL;
    this->efx_properties_map["EFX_REVERB_PRESET_MOOD_MEMORY"] = EFX_REVERB_PRESET_MOOD_MEMORY;
    this->efx_properties_map["EFX_REVERB_PRESET_DRIVING_COMMENTATOR"] = EFX_REVERB_PRESET_DRIVING_COMMENTATOR;
    this->efx_properties_map["EFX_REVERB_PRESET_DRIVING_PITGARAGE"] = EFX_REVERB_PRESET_DRIVING_PITGARAGE;
    this->efx_properties_map["EFX_REVERB_PRESET_DRIVING_INCAR_RACER"] = EFX_REVERB_PRESET_DRIVING_INCAR_RACER;
    this->efx_properties_map["EFX_REVERB_PRESET_DRIVING_INCAR_SPORTS"] = EFX_REVERB_PRESET_DRIVING_INCAR_SPORTS;
    this->efx_properties_map["EFX_REVERB_PRESET_DRIVING_INCAR_LUXURY"] = EFX_REVERB_PRESET_DRIVING_INCAR_LUXURY;
    this->efx_properties_map["EFX_REVERB_PRESET_DRIVING_TUNNEL"] = EFX_REVERB_PRESET_DRIVING_TUNNEL;
    this->efx_properties_map["EFX_REVERB_PRESET_CITY_STREETS"] = EFX_REVERB_PRESET_CITY_STREETS;
    this->efx_properties_map["EFX_REVERB_PRESET_CITY_SUBWAY"] = EFX_REVERB_PRESET_CITY_SUBWAY;
    this->efx_properties_map["EFX_REVERB_PRESET_CITY_UNDERPASS"] = EFX_REVERB_PRESET_CITY_UNDERPASS;
    this->efx_properties_map["EFX_REVERB_PRESET_CITY_ABANDONED"] = EFX_REVERB_PRESET_CITY_ABANDONED;
}

void SoundManager::setListener(Ogre::Vector3 position, Ogre::Vector3 direction, Ogre::Vector3 up, Ogre::Vector3 velocity)
{
    if (!audio_device)
        return;
    listener_position = position;
    listener_direction = direction;
    listener_up = up;
    recomputeAllSources();

    float orientation[6];
    // direction
    orientation[0] = direction.x;
    orientation[1] = direction.y;
    orientation[2] = direction.z;
    // up
    orientation[3] = up.x;
    orientation[4] = up.y;
    orientation[5] = up.z;

    alListener3f(AL_POSITION, position.x, position.y, position.z);
    alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    alListenerfv(AL_ORIENTATION, orientation);

    if(App::audio_enable_efx->getBool())
    {
        this->updateListenerEffectSlot();
    }
}

void SoundManager::setListenerEnvironment(std::string listener_efx_preset_name)
{
    if(efx_properties_map.find(listener_efx_preset_name) == efx_properties_map.end())
    {
        // LOG("SoundManager: EFX preset `" + listener_efx_preset_name + "` is not available");
        listener_efx_preset_name = ""; // force that no preset is active
    }

    this->listener_efx_preset_name = listener_efx_preset_name;
}

void SoundManager::updateListenerEffectSlot()
{
    if (listener_efx_preset_name.empty())
    {
        alAuxiliaryEffectSloti(listener_slot, AL_EFFECTSLOT_EFFECT, AL_EFFECTSLOT_NULL);
    }
    else
    {
        // create new effect if not existing
        if(!listener_efx_preset_name.empty() && efx_effect_id_map.find(listener_efx_preset_name) == efx_effect_id_map.end())
        {
            efx_effect_id_map[listener_efx_preset_name] = this->CreateAlEffect(&this->efx_properties_map[listener_efx_preset_name]);
        }

        // update air absorption gain hf of effect
        if (efx_reverb_engine == EfxReverbEngine::EAXREVERB)
        {
            alEffectf(efx_effect_id_map[listener_efx_preset_name], AL_EAXREVERB_AIR_ABSORPTION_GAINHF, App::audio_air_absorption_gain_hf->getFloat());
        }
        else if (efx_reverb_engine == EfxReverbEngine::REVERB)
        {
            alEffectf(efx_effect_id_map[listener_efx_preset_name], AL_REVERB_AIR_ABSORPTION_GAINHF, App::audio_air_absorption_gain_hf->getFloat());
        }

        // early reflections panning, delay and strength
        if(
           App::audio_enable_reflection_panning->getBool() &&
           efx_reverb_engine == EfxReverbEngine::EAXREVERB &&
           App::app_state->getEnum<AppState>() == AppState::SIMULATION // required to avoid crash when returning to main menu
          )
        {
            /*
             * Detect surfaces close to the listener and pan and delay early reflections accordingly.
             * Use ray casting to probe for a collision up to max_distance to each side of the listener.
             */
            const float max_distance = 2.0f;
            const float reflections_gain_boost_max = 0.316f; // 1 db
            float reflections_gain;
            float reflection_delay;
            float magnitude;

            Ogre::Vector3 reflection_panning_direction = { 0.0f, 0.0f, 0.0f};

            /*
             * To detect surfaces around the listener within the vicinity of
             * max_distance, we cast rays in a 360° circle around the listener
             * on a horizontal and vertical plane.
             */
            float angle_step_size = 90;
            float closest_surface_distance = std::numeric_limits<float>::max();
            int collision_count = 0;

            // surface detection on horizontal plane
            for (float angle = 0; angle < 360; angle += angle_step_size)
            {
                // rotate listener_direction vector around listener_up vector based on angle
                Ogre::Vector3 raycast_direction = Quaternion(Ogre::Degree(angle), listener_up) * listener_direction;
                //LOG("SoundManager: ray(hor): " + std::to_string(raycast_direction.x) + " " + std::to_string(raycast_direction.y) + " " + std::to_string(raycast_direction.z));
                raycast_direction.normalise();
                Ray ray = Ray(listener_position, raycast_direction * 2.0f * max_distance);
                // Ogre::Vector3 debug = ray.getDirection();
                // LOG("SoundManager: ray(hor): " + std::to_string(debug.x) + " " + std::to_string(debug.y) + " " + std::to_string(debug.z) + " " + std::to_string(ray.getDirection().length()));
                std::pair<bool, Ogre::Real> intersection = App::GetGameContext()->GetTerrain()->GetCollisions()->intersectsTris(ray);

                if (intersection.first) // the ray hit something
                {
                    if (intersection.second > max_distance) { continue; }
                    LOG("SoundManager: hit(hor): " + std::to_string(angle) + "dist: " + std::to_string(intersection.second));
                    collision_count++;
                    // add direction to the panning vector weighted by distance
                    reflection_panning_direction += (1.0f - intersection.second / max_distance) * raycast_direction;
                    closest_surface_distance = std::min(intersection.second, closest_surface_distance);
                }
            }

            // surface detection on vertical plane
            angle_step_size = 180;
            for (float angle = 0; angle < 360; angle += angle_step_size)
            {
                // rotate listener_up vector around listener_direction vector based on angle
                Ogre::Vector3 raycast_direction = Quaternion(Ogre::Degree(angle), listener_direction) * listener_up;
                raycast_direction.normalise();
                //LOG("SoundManager: ray(vert): " + std::to_string(raycast_direction.x) + " " + std::to_string(raycast_direction.y) + " " + std::to_string(raycast_direction.z));
                Ray ray = Ray(listener_position, raycast_direction * 2.0f * max_distance);
                // Ogre::Vector3 debug = ray.getDirection();
                // LOG("SoundManager: ray(vert): " + std::to_string(debug.x) + " " + std::to_string(debug.y) + " " + std::to_string(debug.z) + " " + std::to_string(ray.getDirection().length()));
                std::pair<bool, Ogre::Real> intersection = App::GetGameContext()->GetTerrain()->GetCollisions()->intersectsTris(ray);

                if (intersection.first) // the ray hit something
                {
                    if (intersection.second > max_distance) { continue; }
                    LOG("SoundManager: hit(vert): " + std::to_string(angle) + "dist: " + std::to_string(intersection.second));
                    collision_count++;
                    reflection_panning_direction += (1.0f - intersection.second / max_distance) * raycast_direction;
                    closest_surface_distance = std::min(intersection.second, closest_surface_distance);
                }
            }

            if (collision_count == 0) // no nearby surface detected
            {
                // reset values to the original of the preset since there are no nearby surfaces
                reflection_delay = efx_properties_map[listener_efx_preset_name].flReflectionsDelay;
                reflections_gain = efx_properties_map[listener_efx_preset_name].flReflectionsGain;
            }
            else // at least one nearby surface was detected
            {
                // set delay based on distance to the closest surface
                reflection_delay = closest_surface_distance / getSpeedOfSound();

                // we assume that surfaces further away cause less focussed reflections
                magnitude = 1.0f - reflection_panning_direction.length() / max_distance;
LOG("SoundManager: mag: " + std::to_string(magnitude));
                reflections_gain = 3.15f;
                // reflections_gain = std::min(
                //     (efx_properties_map[listener_efx_preset_name].flReflectionsGain
                //        + reflections_gain_boost_max
                //        - (reflections_gain_boost_max * magnitude)),
                //      3.16f);
            }

            /*
             * The EAXREVERB panning vectors do not take the 3D listener orientation into account. Hence; we need to
             * transform the reflection_panning_direction vector to the user-relative EAXREVERB reflection-panning vector
             * it is also necessary to invert z since EAXREVERB panning vectors use a left-handed coordinate system
             */

            reflection_panning_direction.normalise();
            Quaternion horizontal_rotation = listener_direction.getRotationTo(Ogre::Vector3::UNIT_Z, listener_direction);
            Quaternion vertical_rotation = listener_up.getRotationTo(Ogre::Vector3::UNIT_Y, listener_up);
            Ogre::Vector3 reflection_panning_vector = horizontal_rotation * vertical_rotation * reflection_panning_direction;
            reflection_panning_vector.z = -reflection_panning_vector.z;
            reflection_panning_vector.normalise();
            reflection_panning_vector *= magnitude;

            float eaxreverb_reflection_panning_vector[3] =
                { reflection_panning_vector.x,
                  reflection_panning_vector.y,
                 -reflection_panning_vector.z };
LOG("SoundManager: set pan: " + std::to_string(eaxreverb_reflection_panning_vector[0]) + " " + std::to_string(eaxreverb_reflection_panning_vector[1]) + " " + std::to_string(eaxreverb_reflection_panning_vector[2]));
LOG("SoundManager: set delay: " + std::to_string(reflection_delay));
LOG("SoundManager: set gain: " + std::to_string(reflections_gain));
            alEffectfv(efx_effect_id_map[listener_efx_preset_name], AL_EAXREVERB_REFLECTIONS_PAN, eaxreverb_reflection_panning_vector);
            alEffectf(efx_effect_id_map[listener_efx_preset_name], AL_EAXREVERB_REFLECTIONS_DELAY, reflection_delay);
            alEffectf(efx_effect_id_map[listener_efx_preset_name], AL_EAXREVERB_REFLECTIONS_GAIN, reflections_gain);
        }

        // update the effect on the listener effect slot
        alAuxiliaryEffectSloti(listener_slot, AL_EFFECTSLOT_EFFECT, efx_effect_id_map[listener_efx_preset_name]);
    }
}

ALuint SoundManager::CreateAlEffect(const EFXEAXREVERBPROPERTIES* efx_properties)
{
    ALuint effect = 0;
    ALenum error;

    alGenEffects(1, &effect);

    switch (efx_reverb_engine)
    {
        case EfxReverbEngine::EAXREVERB:
            alEffecti(effect,  AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

            alEffectf( effect,  AL_EAXREVERB_DENSITY,                efx_properties->flDensity);
            alEffectf( effect,  AL_EAXREVERB_DIFFUSION,              efx_properties->flDiffusion);
            alEffectf( effect,  AL_EAXREVERB_GAIN,                   efx_properties->flGain);
            alEffectf( effect,  AL_EAXREVERB_GAINHF,                 efx_properties->flGainHF);
            alEffectf( effect,  AL_EAXREVERB_GAINLF,                 efx_properties->flGainLF);
            alEffectf( effect,  AL_EAXREVERB_DECAY_TIME,             efx_properties->flDecayTime);
            alEffectf( effect,  AL_EAXREVERB_DECAY_HFRATIO,          efx_properties->flDecayHFRatio);
            alEffectf( effect,  AL_EAXREVERB_DECAY_LFRATIO,          efx_properties->flDecayLFRatio);
            alEffectf( effect,  AL_EAXREVERB_REFLECTIONS_GAIN,       efx_properties->flReflectionsGain);
            alEffectf( effect,  AL_EAXREVERB_REFLECTIONS_DELAY,      efx_properties->flReflectionsDelay);
            alEffectfv(effect,  AL_EAXREVERB_REFLECTIONS_PAN,        efx_properties->flReflectionsPan);
            alEffectf( effect,  AL_EAXREVERB_LATE_REVERB_GAIN,       efx_properties->flLateReverbGain);
            alEffectf( effect,  AL_EAXREVERB_LATE_REVERB_DELAY,      efx_properties->flLateReverbDelay);
            alEffectfv(effect,  AL_EAXREVERB_LATE_REVERB_PAN,        efx_properties->flLateReverbPan);
            alEffectf( effect,  AL_EAXREVERB_ECHO_TIME,              efx_properties->flEchoTime);
            alEffectf( effect,  AL_EAXREVERB_ECHO_DEPTH,             efx_properties->flEchoDepth);
            alEffectf( effect,  AL_EAXREVERB_MODULATION_TIME,        efx_properties->flModulationTime);
            alEffectf( effect,  AL_EAXREVERB_MODULATION_DEPTH,       efx_properties->flModulationDepth);
            alEffectf( effect,  AL_EAXREVERB_AIR_ABSORPTION_GAINHF,  efx_properties->flAirAbsorptionGainHF);
            alEffectf( effect,  AL_EAXREVERB_HFREFERENCE,            efx_properties->flHFReference);
            alEffectf( effect,  AL_EAXREVERB_LFREFERENCE,            efx_properties->flLFReference);
            alEffectf( effect,  AL_EAXREVERB_ROOM_ROLLOFF_FACTOR,    efx_properties->flRoomRolloffFactor);
            alEffecti( effect,  AL_EAXREVERB_DECAY_HFLIMIT,          efx_properties->iDecayHFLimit);

            break;
        case EfxReverbEngine::REVERB:
            alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);

            alEffectf(effect, AL_REVERB_DENSITY,                efx_properties->flDensity);
            alEffectf(effect, AL_REVERB_DIFFUSION,              efx_properties->flDiffusion);
            alEffectf(effect, AL_REVERB_GAIN,                   efx_properties->flGain);
            alEffectf(effect, AL_REVERB_GAINHF,                 efx_properties->flGainHF);
            alEffectf(effect, AL_REVERB_DECAY_TIME,             efx_properties->flDecayTime);
            alEffectf(effect, AL_REVERB_DECAY_HFRATIO,          efx_properties->flDecayHFRatio);
            alEffectf(effect, AL_REVERB_REFLECTIONS_GAIN,       efx_properties->flReflectionsGain);
            alEffectf(effect, AL_REVERB_REFLECTIONS_DELAY,      efx_properties->flReflectionsDelay);
            alEffectf(effect, AL_REVERB_LATE_REVERB_GAIN,       efx_properties->flLateReverbGain);
            alEffectf(effect, AL_REVERB_LATE_REVERB_DELAY,      efx_properties->flLateReverbDelay);
            alEffectf(effect, AL_REVERB_AIR_ABSORPTION_GAINHF,  efx_properties->flAirAbsorptionGainHF);
            alEffectf(effect, AL_REVERB_ROOM_ROLLOFF_FACTOR,    efx_properties->flRoomRolloffFactor);
            alEffecti(effect, AL_REVERB_DECAY_HFLIMIT,          efx_properties->iDecayHFLimit);

            break;
        case EfxReverbEngine::NONE:
        default:
            LOG("SoundManager: No usable reverb engine set, not creating reverb effect");
    }

    error = alGetError();
    if(error != AL_NO_ERROR)
    {
        LOG("SoundManager: Could not create EFX effect:" + error);

        if(alIsEffect(effect))
            alDeleteEffects(1, &effect);
        return 0;
    }

    return effect;
}

bool compareByAudibility(std::pair<int, float> a, std::pair<int, float> b)
{
    return a.second > b.second;
}

// called when the camera moves
void SoundManager::recomputeAllSources()
{
    // Creates this issue: https://github.com/RigsOfRods/rigs-of-rods/issues/1054
#if 0
	if (!audio_device) return;

	for (int i=0; i < audio_buffers_in_use_count; i++)
	{
		audio_sources[i]->computeAudibility(listener_position);
		audio_sources_most_audible[i].first = i;
		audio_sources_most_audible[i].second = audio_sources[i]->audibility;
	}
    // sort first 'num_hardware_sources' sources by audibility
    // see: https://en.wikipedia.org/wiki/Selection_algorithm
	if ((audio_buffers_in_use_count - 1) > hardware_sources_num)
	{
		std::nth_element(audio_sources_most_audible, audio_sources_most_audible+hardware_sources_num, audio_sources_most_audible + audio_buffers_in_use_count - 1, compareByAudibility);
	}
    // retire out of range sources first
	for (int i=0; i < audio_buffers_in_use_count; i++)
	{
		if (audio_sources[audio_sources_most_audible[i].first]->hardware_index != -1 && (i >= hardware_sources_num || audio_sources_most_audible[i].second == 0))
			retire(audio_sources_most_audible[i].first);
	}
    // assign new sources
	for (int i=0; i < std::min(audio_buffers_in_use_count, hardware_sources_num); i++)
	{
		if (audio_sources[audio_sources_most_audible[i].first]->hardware_index == -1 && audio_sources_most_audible[i].second > 0)
		{
			for (int j=0; j < hardware_sources_num; j++)
			{
				if (hardware_sources_map[j] == -1)
				{
					assign(audio_sources_most_audible[i].first, j);
					break;
				}
			}
		}
	}
#endif

    if(App::audio_enable_efx->getBool())
    {
        for(hardware_sources_num = 0; hardware_sources_num < MAX_HARDWARE_SOURCES; hardware_sources_num++)
        {
            // update air absorption factor
            alSourcef(hardware_sources[hardware_sources_num], AL_AIR_ABSORPTION_FACTOR, App::audio_air_absorption_factor->getFloat());

            if(App::audio_enable_obstruction->getBool())
            {
                /*
                    Check whether the source is obstructed and filter and attenuate it accordingly.
                    Currently, only the change in timbre of the sound is simulated.
                    TODO: Simulate diffraction path.
                */

                // find Sound the hardware_source belongs to
                SoundPtr corresponding_sound = nullptr;
                for(SoundPtr sound : audio_sources)
                {
                    if(sound != nullptr)
                    {
                        if (sound->hardware_index == hardware_sources_num)
                        {
                            corresponding_sound = sound;
                            break;
                        }
                    }
                }

                if (corresponding_sound != nullptr)
                {
                    Ray direct_path_to_sound = Ray(listener_position, corresponding_sound->getPosition());
                    std::pair<bool, Ogre::Real> intersection = App::GetGameContext()->GetTerrain()->GetCollisions()->intersectsTris(direct_path_to_sound);

                    /*
                        TODO: Also check if trucks are obstructing the sound.
                        Trucks shouldn't obstruct their own sound sources since the obstruction is most likely
                        already contained in the recording.
                        If the obstacle is the sound source's own truck, we should still check for other obstacles.
                    */

                    if(intersection.first) // sound is obstructed
                    {
                        // Apply obstruction filter to the source
                        alSourcei(hardware_sources[hardware_sources_num], AL_DIRECT_FILTER, efx_outdoor_obstruction_lowpass_filter_id);
                    }
                    else
                    {
                        // reset direct filter for the source in case it has been set previously
                        alSourcei(hardware_sources[hardware_sources_num], AL_DIRECT_FILTER, AL_FILTER_NULL);
                    }
                    corresponding_sound = nullptr;
                }
            }
        }
    }
}

void SoundManager::recomputeSource(int source_index, int reason, float vfl, Vector3* vvec)
{
    if (!audio_device)
        return;
    audio_sources[source_index]->computeAudibility(listener_position);

    if (audio_sources[source_index]->audibility == 0.0f)
    {
        if (audio_sources[source_index]->hardware_index != -1)
        {
            // retire the source if it is currently assigned
            retire(source_index);
        }
    }
    else
    {
        // this is a potentially audible m_audio_sources[source_index]
        if (audio_sources[source_index]->hardware_index != -1)
        {
            ALuint hw_source = hardware_sources[audio_sources[source_index]->hardware_index];
            // m_audio_sources[source_index] already playing
            // update the AL settings
            switch (reason)
            {
            case Sound::REASON_PLAY: alSourcePlay(hw_source);
                break;
            case Sound::REASON_STOP: alSourceStop(hw_source);
                break;
            case Sound::REASON_GAIN: alSourcef(hw_source, AL_GAIN, vfl * App::audio_master_volume->getFloat());
                break;
            case Sound::REASON_LOOP: alSourcei(hw_source, AL_LOOPING, (vfl > 0.5) ? AL_TRUE : AL_FALSE);
                break;
            case Sound::REASON_PTCH: alSourcef(hw_source, AL_PITCH, vfl);
                break;
            case Sound::REASON_POSN: alSource3f(hw_source, AL_POSITION, vvec->x, vvec->y, vvec->z);
                break;
            case Sound::REASON_VLCT: alSource3f(hw_source, AL_VELOCITY, vvec->x, vvec->y, vvec->z);
                break;
            default: break;
            }
        }
        else
        {
            // try to make it play by the hardware
            // check if there is one free m_audio_sources[source_index] in the pool
            if (hardware_sources_in_use_count < hardware_sources_num)
            {
                for (int i = 0; i < hardware_sources_num; i++)
                {
                    if (hardware_sources_map[i] == -1)
                    {
                        assign(source_index, i);
                        break;
                    }
                }
            }
            else
            {
                // now, compute who is the faintest
                // note: we know the table m_hardware_sources_map is full!
                float fv = 1.0f;
                int al_faintest = 0;
                for (int i = 0; i < hardware_sources_num; i++)
                {
                    if (hardware_sources_map[i] >= 0 && audio_sources[hardware_sources_map[i]]->audibility < fv)
                    {
                        fv = audio_sources[hardware_sources_map[i]]->audibility;
                        al_faintest = i;
                    }
                }
                // check to ensure that the sound is louder than the faintest sound currently playing
                if (fv < audio_sources[source_index]->audibility)
                {
                    // this new m_audio_sources[source_index] is louder than the faintest!
                    retire(hardware_sources_map[al_faintest]);
                    assign(source_index, al_faintest);
                }
                // else this m_audio_sources[source_index] is too faint, we don't play it!
            }
        }
    }
}

void SoundManager::assign(int source_index, int hardware_index)
{
    if (!audio_device)
        return;
    audio_sources[source_index]->hardware_index = hardware_index;
    hardware_sources_map[hardware_index] = source_index;

    ALuint hw_source = hardware_sources[hardware_index];
    SoundPtr& audio_source = audio_sources[source_index];

    // the hardware source is supposed to be stopped!
    alSourcei(hw_source, AL_BUFFER, audio_source->buffer);
    alSourcef(hw_source, AL_GAIN, audio_source->gain * App::audio_master_volume->getFloat());
    alSourcei(hw_source, AL_LOOPING, (audio_source->loop) ? AL_TRUE : AL_FALSE);
    alSourcef(hw_source, AL_PITCH, audio_source->pitch);
    alSource3f(hw_source, AL_POSITION, audio_source->position.x, audio_source->position.y, audio_source->position.z);
    alSource3f(hw_source, AL_VELOCITY, audio_source->velocity.x, audio_source->velocity.y, audio_source->velocity.z);

    if (audio_source->should_play)
    {
        alSourcePlay(hw_source);
    }

    hardware_sources_in_use_count++;
}

void SoundManager::retire(int source_index)
{
    if (!audio_device)
        return;
    if (audio_sources[source_index]->hardware_index == -1)
        return;
    alSourceStop(hardware_sources[audio_sources[source_index]->hardware_index]);
    hardware_sources_map[audio_sources[source_index]->hardware_index] = -1;
    audio_sources[source_index]->hardware_index = -1;
    hardware_sources_in_use_count--;
}

void SoundManager::pauseAllSounds()
{
    if (!audio_device)
        return;
    // no mutex needed
    alListenerf(AL_GAIN, 0.0f);
}

void SoundManager::resumeAllSounds()
{
    if (!audio_device)
        return;
    // no mutex needed
    alListenerf(AL_GAIN, App::audio_master_volume->getFloat());
}

void SoundManager::setMasterVolume(float v)
{
    if (!audio_device)
        return;
    // no mutex needed
    App::audio_master_volume->setVal(v); // TODO: Use 'pending' mechanism and set externally, only 'apply' here.
    alListenerf(AL_GAIN, v);
}

SoundPtr SoundManager::createSound(String filename, Ogre::String resource_group_name /* = "" */)
{
    if (!audio_device)
        return NULL;

    if (audio_buffers_in_use_count >= MAX_AUDIO_BUFFERS)
    {
        LOG("SoundManager: Reached MAX_AUDIO_BUFFERS limit (" + TOSTRING(MAX_AUDIO_BUFFERS) + ")");
        return NULL;
    }

    ALuint buffer = 0;

    // is the file already loaded?
    for (int i = 0; i < audio_buffers_in_use_count; i++)
    {
        if (filename == audio_buffer_file_name[i])
        {
            buffer = audio_buffers[i];
            break;
        }
    }

    if (!buffer)
    {
        // load the file
        alGenBuffers(1, &audio_buffers[audio_buffers_in_use_count]);
        if (loadWAVFile(filename, audio_buffers[audio_buffers_in_use_count], resource_group_name))
        {
            // there was an error!
            alDeleteBuffers(1, &audio_buffers[audio_buffers_in_use_count]);
            audio_buffer_file_name[audio_buffers_in_use_count] = "";
            return NULL;
        }
        buffer = audio_buffers[audio_buffers_in_use_count];
        audio_buffer_file_name[audio_buffers_in_use_count] = filename;
    }

    audio_sources[audio_buffers_in_use_count] = new Sound(buffer, this, audio_buffers_in_use_count);

    return audio_sources[audio_buffers_in_use_count++];
}

bool SoundManager::loadWAVFile(String filename, ALuint buffer, Ogre::String resource_group_name /*= ""*/)
{
    if (!audio_device)
        return true;
    LOG("Loading WAV file "+filename);

    // create the Stream
    ResourceGroupManager* rgm = ResourceGroupManager::getSingletonPtr();
    if (resource_group_name == "")
    {
        resource_group_name = rgm->findGroupContainingResource(filename);
    }
    DataStreamPtr stream = rgm->openResource(filename, resource_group_name);

    // load RIFF/WAVE
    char magic[5];
    magic[4] = 0;
    unsigned int lbuf; // uint32_t
    unsigned short sbuf; // uint16_t

    // check magic
    if (stream->read(magic, 4) != 4)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    if (String(magic) != String("RIFF"))
    {
        LOG("Invalid WAV file (no RIFF): "+filename);
        return true;
    }
    // skip 4 bytes (magic)
    stream->skip(4);
    // check file format
    if (stream->read(magic, 4) != 4)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    if (String(magic) != String("WAVE"))
    {
        LOG("Invalid WAV file (no WAVE): "+filename);
        return true;
    }
    // check 'fmt ' sub chunk (1)
    if (stream->read(magic, 4) != 4)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    if (String(magic) != String("fmt "))
    {
        LOG("Invalid WAV file (no fmt): "+filename);
        return true;
    }
    // read (1)'s size
    if (stream->read(&lbuf, 4) != 4)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    unsigned long subChunk1Size = lbuf;
    if (subChunk1Size < 16)
    {
        LOG("Invalid WAV file (invalid subChunk1Size): "+filename);
        return true;
    }
    // check PCM audio format
    if (stream->read(&sbuf, 2) != 2)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    unsigned short audioFormat = sbuf;
    if (audioFormat != 1)
    {
        LOG("Invalid WAV file (invalid audioformat "+TOSTRING(audioFormat)+"): "+filename);
        return true;
    }
    // read number of channels
    if (stream->read(&sbuf, 2) != 2)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    unsigned short channels = sbuf;
    // read frequency (sample rate)
    if (stream->read(&lbuf, 4) != 4)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    unsigned long freq = lbuf;
    // skip 6 bytes (Byte rate (4), Block align (2))
    stream->skip(6);
    // read bits per sample
    if (stream->read(&sbuf, 2) != 2)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    unsigned short bps = sbuf;
    // check 'data' sub chunk (2)
    if (stream->read(magic, 4) != 4)
    {
        LOG("Could not read file "+filename);
        return true;
    }
    if (String(magic) != String("data") && String(magic) != String("fact"))
    {
        LOG("Invalid WAV file (no data/fact): "+filename);
        return true;
    }
    // fact is an option section we don't need to worry about
    if (String(magic) == String("fact"))
    {
        stream->skip(8);
        // now we should hit the data chunk
        if (stream->read(magic, 4) != 4)
        {
            LOG("Could not read file "+filename);
            return true;
        }
        if (String(magic) != String("data"))
        {
            LOG("Invalid WAV file (no data): "+filename);
            return true;
        }
    }
    // the next four bytes are the remaining size of the file
    if (stream->read(&lbuf, 4) != 4)
    {
        LOG("Could not read file "+filename);
        return true;
    }

    unsigned long dataSize = lbuf;
    int format = 0;

    if (channels == 1 && bps == 8)
        format = AL_FORMAT_MONO8;
    else if (channels == 1 && bps == 16)
        format = AL_FORMAT_MONO16;
    else if (channels == 2 && bps == 8)
        format = AL_FORMAT_STEREO16;
    else if (channels == 2 && bps == 16)
        format = AL_FORMAT_STEREO16;
    else
    {
        LOG("Invalid WAV file (wrong channels/bps): "+filename);
        return true;
    }

    if (channels != 1) LOG("Invalid WAV file: the file needs to be mono, and nothing else. Will try to continue anyways ...");

    // ok, creating buffer
    void* bdata = malloc(dataSize);
    if (!bdata)
    {
        LOG("Memory error reading file "+filename);
        return true;
    }
    if (stream->read(bdata, dataSize) != dataSize)
    {
        LOG("Could not read file "+filename);
        free(bdata);
        return true;
    }

    //LOG("alBufferData: format "+TOSTRING(format)+" size "+TOSTRING(dataSize)+" freq "+TOSTRING(freq));
    alGetError(); // Reset errors
    ALint error;
    alBufferData(buffer, format, bdata, dataSize, freq);
    error = alGetError();

    free(bdata);
    // stream will be closed by itself

    if (error != AL_NO_ERROR)
    {
        LOG("OpenAL error while loading buffer for "+filename+" : "+TOSTRING(error));
        return true;
    }

    return false;
}

#endif // USE_OPENAL
