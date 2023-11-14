#pragma once

//* Video, audio, and input backend. Uses SDL2.
#include <SDL2/SDL.h>

//* Debuggin'
#define USE_BLIP_ADD_DELTA_FAST
//#define USE_VSYNC

extern bool bUserQuits;
extern bool exitFlag;
extern bool bVerbose;
extern bool bExtraVerbose;

extern bool bRunTests;
extern bool bForcePAL;
extern bool bForceNTSC;

extern SDL_mutex *frame_lock;
extern SDL_mutex *event_lock;
extern SDL_Texture *overlay_tex;

extern void process_events();
extern void add_controller( int device_index);
extern void remove_controller(SDL_JoystickID instance_id);
extern bool get_controller_index(SDL_JoystickID instance_id, int *controller_index);

void init_sdl();
void deinit_sdl();
//* Called from the emulation thread to cause the SDL thread to exit
void exit_sdl_thread();
//* SDL rendering thread. Runs separately from the emulation thread.
void sdl_thread();
void RunEmulation();
void put_pixel(unsigned x, unsigned y, uint32_t color);
void draw_frame();

int const sample_rate = 44100; 
Uint16 const sdl_audio_buffer_size = 2048;

//* Protect the audio buffer from concurrent access by the emulation thread and SDL
void lock_audio();
void unlock_audio();

//* Stop and start audio playback in SDL
void start_audio_playback();
void stop_audio_playback();
