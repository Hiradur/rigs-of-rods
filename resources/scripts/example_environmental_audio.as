/*
    ---------------------------------------------------------------------------
                  Project Rigs of Rods (www.rigsofrods.org)
                            
                    Example script for environmental audio
                                
    This program showcases all the various things you can do with environmental audio:
     * get current speed of sound
     * change speed of sound
     * get current doppler factor
     * change doppler factor
     * set EFX preset to a preset shipped with RoR by default
     * Register custom EFX preset
     * change EFX preset to custom preset
     * unset EFX preset
     * deregister EFX preset
     * hand back environmental audio control to RoR
    
    Scripting documentation:
    https://developer.rigsofrods.org/d4/d07/group___script2_game.html
     
    ---------------------------------------------------------------------------
*/

/*
    ---------------------------------------------------------------------------
    Global variables
*/
float       g_total_seconds = 0;
CVarClass@  g_efx_reverb_engine = console.cVarGet("audio_efx_reverb_engine");
enum EnvironmentalAudioExampleScriptState =
{
    DEMO_SPEED_OF_SOUND,
    DEMO_DOPPLER_FACTOR,
    DEMO_RESET_DOPPLER,
    DEMO_EFX_SHIPPED_PRESET,
    DEMO_EFX_SHIPPED_PRESET_EFFECT,
    DEMO_EFX_CUSTOM_PRESET,
    INITIATE_SHUTDOWN,
    FINISHED
}
EnvironmentalAudioExampleScriptState script_state = DEMO_SPEED_OF_SOUND;
float seconds_since_last_state_change = 0;
float state_change_timestamp;

void main()
{
    
    /*
     * This is very important: If you want to exert control over environmental audio, you
     * have to disable RoR's automatic environmental audio.
     * Please note that this means that these properties are no longer changed by the engine:
     *   - speed of sound
     *   - doppler factor
     *   - EFX Presets for the listener
     * However, the following are still managed by the engine unless they are disabled via CVar
     * (which you can do in a script, but you should reset them to their original value on shutdown):
     *   - obstruction
     *
     */
    console.cvcVarSet(audio_engine_controls_environmental_audio, false);

    /*
     * If you want to use a custom OpenAL EFX Preset, it has to be defined first
     * and then registered with the reverb engine. It makes sense to do this in
     * the main function and activate the preset from another function.
     * For a description of the properties see the OpenAL Effects Extensions Guide:
     * https://openal-soft.org/misc-downloads/Effects%20Extension%20Guide.pdf
     */
    EFXEAXREVERBPROPERTIES ENVIRONMENTAL_AUDIO_SCRIPT_DEMO_PRESET = 
        { 
          1.0000f,                        // Density
          1.0000f,                        // Diffusion
          0.3162f,                        // Gain
          0.8765f,                        // GainHF
          1.0000f,                        // GainLF
          1.9000f,                        // DecayTime
          0.7500f,                        // DecayHFRatio
          1.0000f,                        // DecayLFRatio
          0.0533f,                        // ReflectionsGain
          0.0100f,                        // ReflectionsDelay
          { 0.0000f, 0.0000f, 0.2500f },  // ReflectionsPan
          1.2589f,                        // LateReverbGain
          0.2400f,                        // LateReverbDelay
          { 0.0000f, 0.0000f, -0.2500f }, // LateReverbPan
          0.2500f,                        // EchoTime
          0.7000f,                        // EchoDepth
          0.2500f,                        // ModulationTime
          0.2000f,                        // ModulationDepth
          0.9943f,                        // AirAbsorptionGainHF
          5000.0000f,                     // HFReference
          250.0000f,                      // LFReference
          0.0000f,                        // RoomRolloffFactor
          0x1                             // iDecayHFLimit
        }

    // now, register the preset with the audio engine
    registerEfxPreset("ENVIRONMENTAL_AUDIO_SCRIPT_DEMO_PRESET", ENVIRONMENTAL_AUDIO_SCRIPT_DEMO_PRESET);
}

void frameStep(float dt)
{
    /* Glossary:
     *   Listener: In OpenAL terms, the listener is where your ingame-ears are. 
     *             In RoR, the listener is not the avatar but the camera!
     *   Doppler Effect/Shift: The perceived shift in frequency of sounds caused 
     *             by the movement of the source relative to the listener.
     */

    /* 
     * State tracking for the purpose of this demo script:
     * In this example script, everything is triggered based on time. 
     * In productive scripts, you might want to trigger changes to environmental
     * audio based on events or position of the listener
     */
    if (script_state == FINISHED)
    {
        return;
    }

    g_total_seconds += dt;
    seconds_since_last_state_change = g_total_seconds - state_change_timestamp;

    if(seconds_since_last_state_change > 5.0f || script_state == DEMO_SPEED_OF_SOUND)
    {
        switch(script_state)
        {
            case DEMO_SPEED_OF_SOUND:
                /*
                * Dealing with the Speed of Sound
                * The speed of sound is dependent on the properties of the medium the listener is in.
                * For example, these properties impact the speed of sound:
                * - is it air or water or something else?
                * - temperature
                * - humidity
                * ...
                * OpenAL does not delay sound based on distance and speed of sound. However, the 
                * speed of sound is used to calculate the doppler shift.
                * Typically you want to modify this based on weather conditions (e.g. fog) and medium of the listener.
                */
                log("Info: The currently select reverb engine is: " + g_efx_reverb_engine);

                float original_speed_of_sound = getSpeedOfSound();
                log("Current speed of sound in m/s is: " + original_speed_of_sound);
                float desired_speed_of_sound = 100.0f;
                log("Setting speed of sound to: " + desired_speed_of_sound);
                setSpeedOfSound(desired_speed_of_sound);
                log("Current speed of sound in m/s is: " + getSpeedOfSound());
                script_state = DEMO_DOPPLER_FACTOR;
                return;
            case DEMO_DOPPLER_FACTOR:
                /*
                * Dealing with the Doppler Factor
                * The doppler factor is an artifical construct to (de-)emphasize the doppler effect.
                * Internally, it is multiplied with the speed of sound to determine the final strength
                * of the doppler factor. If you want to modify the doppler effect, you should prefer
                * doing this via the doppler factor. The speed of sound should be modifi
                */
                original_doppler_factor = getDopplerFactor();
                log("Current doppler factor is: " + original_doppler_factor);
                desired_doppler_factor = 10.0f;
                log("Setting speed of sound to: " + desired_doppler_factor);
                setDopplerFactor(desired_doppler_factor);
                log("Current doppler factor is: " + getDopplerFactor());
                script_state = DEMO_RESET_DOPPLER;
                return;
            case DEMO_RESET_DOPPLER:
                // reset to the original state
                setSpeedOfSound(original_speed_of_sound);
                setDopplerFactor(original_doppler_factor);
                script_state = DEMO_EFX_SHIPPED_PRESET;
                return;
            case DEMO_EFX_SHIPPED_PRESET:
                /*
                * Dealing with EFX Presets
                * EFX presets are used to simulate the reverberation of different environments
                * or to create specific effects.
                */
                // activate a preset that ships with RoR by default
                setListenerEnvironment("EFX_REVERB_PRESET_SEWERPIPE");
                script_state = DEMO_EFX_SHIPPED_PRESET_EFFECT;
                return;
            case DEMO_EFX_SHIPPED_PRESET_EFFECT:
                // activate a preset that creates an effect
                setListenerEnvironment("EFX_REVERB_PRESET_PSYCHOTIC");
                script_state = DEMO_EFX_CUSTOM_PRESET;
                return;
            case DEMO_EFX_CUSTOM_PRESET:
                // set our own custom preset
                setListenerEnvironment("ENVIRONMENTAL_AUDIO_SCRIPT_DEMO_PRESET");
                script_state = INITIATE_SHUTDOWN;
                return;
            case INITIATE_SHUTDOWN:     
                shutdown();
                break;
        }
    }
}

void shutdown()
{
    // deactivate reverb
    setListenerEnvironment("");
    
    // unload the EFX preset we registered to free memory
    unregisterEfxPreset("ENVIRONMENTAL_AUDIO_SCRIPT_DEMO_PRESET");

    /* 
     * When the script exits, you usually want to reactivate RoR's automatic 
     * environmental audio control.
     */
    console.cvcVarSet(audio_engine_controls_environmental_audio, true);
}
