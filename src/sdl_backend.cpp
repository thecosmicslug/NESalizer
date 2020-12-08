#include <string>
#include <iostream>

#include "common.h"

#include "audio.h"
#include "cpu.h"
#include "input.h"

#include "save_states.h"
#include "sdl_backend.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

static SDL_Window   *screen;
static SDL_Renderer *renderer;
static SDL_Texture  *screen_tex;
static Uint32 *front_buffer;
static Uint32 *back_buffer;

static bool ready_to_draw_new_frame;
static bool frame_available;
static bool pending_sdl_thread_exit;
static SDL_cond  *frame_available_cond;

SDL_mutex *frame_lock;
SDL_mutex   *event_lock;

static SDL_AudioDeviceID audio_device_id;

// Framerate control:
const int FPS = 60;
const int DELAY = 100.0f / FPS;

const unsigned WIDTH = 256;
const unsigned HEIGHT = 240;

struct Controller_t
{
	enum Type {
		k_Available,
		k_Joystick,
		k_Gamepad,
	} type;

	SDL_JoystickID instance_id;
	SDL_Joystick *joystick;
	SDL_GameController *gamepad;
};
static Controller_t controllers[2];

static void process_events();

void lock_audio() { SDL_LockAudioDevice(audio_device_id); }
void unlock_audio() { SDL_UnlockAudioDevice(audio_device_id); }

void start_audio_playback() { SDL_PauseAudioDevice(audio_device_id, 0); }
void stop_audio_playback() { SDL_PauseAudioDevice(audio_device_id, 1); }

void put_pixel(unsigned x, unsigned y, uint32_t color) {
    assert(x < 256);
    assert(y < 240);
    back_buffer[256*y + x] = color;
}

void draw_frame() {

    uint32_t frameStart, frameTime;
    frameStart = SDL_GetTicks();

    SDL_LockMutex(frame_lock);
    if (ready_to_draw_new_frame) {
        frame_available = true;
        swap(back_buffer, front_buffer);
        SDL_CondSignal(frame_available_cond);
    } else {
        //printf("dropping frame\n");
    }

    SDL_UnlockMutex(frame_lock);
    // Wait to mantain framerate:
    frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < DELAY) {
        SDL_Delay((int)(DELAY - frameTime));
    }
}

static void audio_callback(void*, Uint8 *stream, int len) {
    assert(len >= 0);
    read_samples((int16_t*)stream, len/sizeof(int16_t));
}

bool saveScreenshot(const std::string &file, SDL_Renderer *renderer ) {
  SDL_Rect _viewport;
  SDL_Surface *_surface = NULL;
  SDL_RenderGetViewport( renderer, &_viewport);
  _surface = SDL_CreateRGBSurface( 0, _viewport.w, _viewport.h, 32, 0, 0, 0, 0 );
  if ( _surface == NULL ) {
    std::cout << "Cannot create SDL_Surface: " << SDL_GetError() << std::endl;
    return false;
   }
  if ( SDL_RenderReadPixels( renderer, NULL, _surface->format->format, _surface->pixels, _surface->pitch ) != 0 ) {
    std::cout << "Cannot read data from SDL_Renderer: " << SDL_GetError() << std::endl;
    SDL_FreeSurface(_surface);
    return false;
  }
  if ( IMG_SavePNG( _surface, file.c_str() ) != 0 ) {
    std::cout << "Cannot save PNG file: " << SDL_GetError() << std::endl;
    SDL_FreeSurface(_surface);
    return false;
  }
  SDL_FreeSurface(_surface);
  return true;
}

static void add_controller(Controller_t::Type type, int device_index)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type == Controller_t::k_Available) {
			if (type == Controller_t::k_Gamepad) {
				controller.gamepad = SDL_GameControllerOpen(device_index);
				if (!controller.gamepad) {
					fprintf(stderr, "Couldn't open gamepad: %s\n", SDL_GetError());
					return;
				}
				controller.joystick = SDL_GameControllerGetJoystick(controller.gamepad);
				printf("Opened game controller %s at index %d\n", SDL_GameControllerName(controller.gamepad), i);
			} else {
				controller.joystick = SDL_JoystickOpen(device_index);
				if (!controller.joystick) {
					fprintf(stderr, "Couldn't open joystick: %s\n", SDL_GetError());
					return;
				}
				printf("Opened joystick %s at index %d\n", SDL_JoystickName(controller.joystick), i);
			}
			controller.type = type;
			controller.instance_id = SDL_JoystickInstanceID(controller.joystick);
			return;
		}
	}
}

static bool get_controller_index(SDL_JoystickID instance_id, int *controller_index)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type != Controller_t::k_Gamepad) {
			continue;
		}
		if (controller.instance_id != instance_id) {
			continue;
		}
		*controller_index = i;
		return true;
	}
	return false;
}

static void remove_controller(Controller_t::Type type, SDL_JoystickID instance_id)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type != type) {
			continue;
		}
		if (controller.instance_id != instance_id) {
			continue;
		}
		if (controller.type == Controller_t::k_Gamepad) {
			SDL_GameControllerClose(controller.gamepad);
		} else {
			SDL_JoystickClose(controller.joystick);
		}
		controller.type = Controller_t::k_Available;
		return;
	}
}

static void process_events() {

    SDL_Event event;
    SDL_LockMutex(event_lock);

    while (SDL_PollEvent(&event)) {
        switch(event.type)
        {
            case SDL_QUIT:
                end_emulation();
                pending_sdl_thread_exit = true;
                break;
            case SDL_CONTROLLERDEVICEADDED:
		        add_controller(Controller_t::k_Gamepad, event.cdevice.which);
		        break;
            case SDL_CONTROLLERDEVICEREMOVED:
		        remove_controller(Controller_t::k_Gamepad, event.cdevice.which);
		        break;
            case SDL_CONTROLLERBUTTONDOWN:
                int controller_index_down;
                if (!get_controller_index(event.cbutton.which, &controller_index_down)) {
                    break;
                }
                switch(event.cbutton.button)
                {
                    case  SDL_CONTROLLER_BUTTON_A:
                        set_button_state(controller_index_down,0);
                        break;
                    case  SDL_CONTROLLER_BUTTON_B:
                        set_button_state(controller_index_down,1);
                        break;
                    case  SDL_CONTROLLER_BUTTON_BACK:
                        set_button_state(controller_index_down,2);
                        break;
                    case  SDL_CONTROLLER_BUTTON_START:
                        set_button_state(controller_index_down,3);
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_UP:
                        set_button_state(controller_index_down,4);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        set_button_state(controller_index_down,5);
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        set_button_state(controller_index_down,6);
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        set_button_state(controller_index_down,7);
                        break;
                    case  SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        // Load Save-state
                        printf("user called load_state()\n");
                        load_state();
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        // Save State
                        printf("user called save_state()\n");
                        save_state();
                        break;
                    case  SDL_CONTROLLER_BUTTON_LEFTSTICK:
                        // Reset the running ROM
                        //printf("User reset the Running ROM!\n");
                        //soft_reset();
                        printf("Saving screenshot!\n");
                        saveScreenshot("nesalizer.png",renderer);
                        break;     
                    case  SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                        // Exit NESalizer!
                        printf("User pressed quit!\n");
                        exit_sdl_thread();
                        deinit_sdl();
                        break;     
                }
		        break;
            case SDL_CONTROLLERBUTTONUP:
                int controller_index_up;
                if (!get_controller_index(event.cbutton.which, &controller_index_up)) {
                    break;
                }
                switch(event.cbutton.button)
                {
                    case  SDL_CONTROLLER_BUTTON_A:
                        clear_button_state(controller_index_up,0);
                        break;
                    case  SDL_CONTROLLER_BUTTON_B:
                        clear_button_state(controller_index_up,1);
                        break;
                    case  SDL_CONTROLLER_BUTTON_BACK:
                        clear_button_state(controller_index_up,2);
                        break;
                    case  SDL_CONTROLLER_BUTTON_START:
                        clear_button_state(controller_index_up,3);
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_UP:
                        clear_button_state(controller_index_up,4);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        clear_button_state(controller_index_up,5);
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        clear_button_state(controller_index_up,6);
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        clear_button_state(controller_index_up,7);
                        break;
                    case  SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                        // Exit NESalizer!
                        printf("User quit!\n");
                        exit_sdl_thread();
                        deinit_sdl();
                        break;     
                }
            break;
        }
    }
    SDL_UnlockMutex(event_lock);
}

void sdl_thread() {
    printf("Entering sdl_thread\n");
    SDL_UnlockMutex(frame_lock);
    for(;;) {
        // Wait for the emulation thread to signal that a frame has completed
        SDL_LockMutex(frame_lock);
        ready_to_draw_new_frame = true;
        while (!frame_available && !pending_sdl_thread_exit)
            SDL_CondWait(frame_available_cond, frame_lock);
        if (pending_sdl_thread_exit) {
            SDL_UnlockMutex(frame_lock);
            pending_sdl_thread_exit = false;
            return;
        }
        frame_available = ready_to_draw_new_frame = false;
        SDL_UnlockMutex(frame_lock);
        process_events();
        // Draw the new frame
        if(SDL_UpdateTexture(screen_tex, 0, front_buffer, 256*sizeof(Uint32))) {
            printf("failed to update screen texture: %s", SDL_GetError());
            exit(1);
        }
        if(SDL_RenderCopy(renderer, screen_tex, 0, 0)) {
            printf("failed to copy rendered frame to render target: %s", SDL_GetError());
            exit(1);
        }
        SDL_RenderPresent(renderer);
    }
    printf("Exiting sdl_thread\n");
}

void exit_sdl_thread() {
    SDL_LockMutex(frame_lock);
    pending_sdl_thread_exit = true;
    SDL_CondSignal(frame_available_cond);
    SDL_UnlockMutex(frame_lock);
}

// Initialization and de-initialization
void init_sdl() {

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("failed to initialize SDL: %s", SDL_GetError());
        exit(1);
    }

    if(!(screen = SDL_CreateWindow(NULL,SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED , 256 , 240 , SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL))) 
    {
        printf("failed to create window: %s", SDL_GetError());
        exit(1);
    }

    if(!(renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE))) 
    {
        printf("failed to create rendering context: %s", SDL_GetError());
        exit(1);
    }

    // Display some information about the renderer
    SDL_RendererInfo renderer_info;
    printf("SDL_GetRendererInfo\n");
    if (SDL_GetRendererInfo(renderer, &renderer_info))
        puts("Failed to get renderer information from SDL");
    else {
        if (renderer_info.name)
            printf("renderer: uses renderer \"%s\"\n", renderer_info.name);
        if (renderer_info.flags & SDL_RENDERER_SOFTWARE)
            puts("renderer: uses software rendering");
        if (renderer_info.flags & SDL_RENDERER_ACCELERATED)
            puts("renderer: uses hardware-accelerated rendering");
        if (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC)
            puts("renderer: uses vsync");
        if (renderer_info.flags & SDL_RENDERER_TARGETTEXTURE)
            puts("renderer: supports rendering to texture");
        printf("renderer: available texture formats:");
        unsigned const n_texture_formats = min(16u, (unsigned)renderer_info.num_texture_formats);
        for (unsigned i = 0; i < n_texture_formats; ++i)
            printf(" %s", SDL_GetPixelFormatName(renderer_info.texture_formats[i]));
        putchar('\n');
    }

    printf("SDL_SetHint\n");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    printf("SDL_CreateTexture\n");
    
    if(!(screen_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888 , SDL_TEXTUREACCESS_STREAMING , 256 , 240))) 
    {
        printf("failed to create texture for screen: %s", SDL_GetError());
        exit(1);
    }

    static Uint32 render_buffers[2][240*256];
    back_buffer  = render_buffers[0];
    front_buffer = render_buffers[1];

    // Audio
    SDL_AudioSpec want;
    SDL_AudioSpec got;

    want.freq     = sample_rate; 
    want.format   = AUDIO_S16LSB;  // AUDIO_S16SYS in original - AUDIO_S16LSB in kevtroots switch port
    want.channels = 1;
    want.samples  = sdl_audio_buffer_size;
    want.callback = audio_callback;

    printf("SDL_OpenAudioDevice\n");
    audio_device_id = SDL_OpenAudioDevice(0, 0, &want, &got, SDL_AUDIO_ALLOW_ANY_CHANGE);
    
    printf("freq: %i, %i\n", want.freq, got.freq);
    printf("format: %i, %i\n", want.format, got.format);
    printf("channels: %i, %i\n", want.channels, got.channels);
    printf("samples: %i, %i\n", want.samples, got.samples);
    
    // Input
    printf("SDL_EventState\n");
    
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONUP  , SDL_IGNORE);
    SDL_EventState(SDL_MOUSEMOTION    , SDL_IGNORE);

    /* Ignore key events */
    SDL_EventState(SDL_KEYDOWN, SDL_IGNORE);
    SDL_EventState(SDL_KEYUP, SDL_IGNORE);

    // Ignore window events for now
    SDL_EventState(SDL_WINDOWEVENT, SDL_IGNORE);

    // SDL thread synchronization
    printf("SDL_CreateMutex\n");
    if(!(event_lock = SDL_CreateMutex())) {
        printf("failed to create event mutex: %s", SDL_GetError());
        exit(1);
    }
    if(!(frame_lock = SDL_CreateMutex())) {
        printf("failed to create frame mutex: %s", SDL_GetError());
        exit(1);
    }
   
    printf("SDL_CreateCond()\n");
    if(!(frame_available_cond = SDL_CreateCond())) {
        printf("failed to create frame condition variable: %s", SDL_GetError());
        exit(1);
    }
}

void deinit_sdl() {
    puts("Shutting down NESalizer!");
    puts("-------------------------------------------------------");
    SDL_DestroyRenderer(renderer); // Also destroys the texture
    SDL_DestroyWindow(screen);
    SDL_DestroyMutex(event_lock);
    SDL_DestroyMutex(frame_lock);
    SDL_DestroyCond(frame_available_cond);
    SDL_CloseAudioDevice(audio_device_id); // Prolly not needed, but play it safe
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    SDL_Quit();
}
