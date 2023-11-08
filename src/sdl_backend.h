#pragma once
#define IMGUI_USER_CONFIG "./nesalizer_imgui_config.h"

//* Video, audio, and input backend. Uses SDL2.
#include <SDL2/SDL.h>

extern bool bUserQuits;

void init_sdl();
void deinit_sdl();

//* SDL rendering thread. Runs separately from the emulation thread.
void sdl_thread();
extern void process_events();
extern void process_gui_events();

void put_pixel(unsigned x, unsigned y, uint32_t color);
void draw_frame();

//* Called from the emulation thread to cause the SDL thread to exit
void exit_sdl_thread();

int const sample_rate = 44100; 
//int const sample_rate = 22050; 
//int const sample_rate = 11025; 
//int const sample_rate = 96000;
//int const sample_rate = 59562;

//Uint16 const sdl_audio_buffer_size = 2048; // 1024; 
Uint16 const sdl_audio_buffer_size = 2048;

//* Protect the audio buffer from concurrent access by the emulation thread and SDL
void lock_audio();
void unlock_audio();

//* Stop and start audio playback in SDL
void start_audio_playback();
void stop_audio_playback();
