#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "Audio.h"
#include <iostream>

ma_engine engine;
ma_sound footstepSound;
ma_sound waterAmbientSound;

bool audioInitialized = false;

void initAudio() {
    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine." << std::endl;
        return;
    }

    // Initialize footsteps (looped)
    result = ma_sound_init_from_file(&engine, "sounds/walking-on-grass.mp3", 0, NULL, NULL, &footstepSound);
    if (result == MA_SUCCESS) {
        ma_sound_set_looping(&footstepSound, MA_TRUE);
        ma_sound_set_volume(&footstepSound, 1.5f); // Make it a bit louder
    } else {
        std::cerr << "Failed to load sounds/walking-on-grass.mp3" << std::endl;
    }

    // Initialize background water (looped)
    result = ma_sound_init_from_file(&engine, "sounds/water.mp3", 0, NULL, NULL, &waterAmbientSound);
    if (result == MA_SUCCESS) {
        ma_sound_set_looping(&waterAmbientSound, MA_TRUE);
        ma_sound_set_volume(&waterAmbientSound, 0.0f); // Start silent, will be updated by distance
        ma_sound_start(&waterAmbientSound); // Play continuously
    } else {
        std::cerr << "Failed to load sounds/water.mp3" << std::endl;
    }

    audioInitialized = true;
}

void cleanupAudio() {
    if (!audioInitialized) return;
    ma_sound_uninit(&footstepSound);
    ma_sound_uninit(&waterAmbientSound);
    ma_engine_uninit(&engine);
}

void playFootstep() {
    if (!audioInitialized) return;
    if (!ma_sound_is_playing(&footstepSound)) {
        ma_sound_start(&footstepSound);
    }
}

void stopFootstep() {
    if (!audioInitialized) return;
    if (ma_sound_is_playing(&footstepSound)) {
        ma_sound_stop(&footstepSound);
    }
}

void setWaterVolume(float volume) {
    if (!audioInitialized) return;
    ma_sound_set_volume(&waterAmbientSound, volume);
}
