#include <SDL2/SDL.h>

#include "common.h"
#include "audio.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "rom.h"
#include "save_states.h"
#include "test.h"
#include "sdl_backend.h"
#include "sdl_frontend.h"

//* SDL Rendering bits
static SDL_Window   *screen;
static SDL_Renderer *renderer;
static SDL_Texture  *screen_tex;
SDL_Texture *overlay_tex;

//* Mutexs for the emulation thread.
SDL_mutex *frame_lock;
SDL_mutex *event_lock;
static SDL_cond  *frame_available_cond;
static SDL_AudioDeviceID audio_device_id;

static bool ready_to_draw_new_frame;
static bool frame_available;
static bool pending_sdl_thread_exit;

//* Our screen buffer
Uint32 *back_buffer __attribute__((aligned(32)));
Uint32 render_buffers[2][240*256] __attribute__((aligned(32)));


//* Configuration flags
bool exitFlag = false;
bool bUserQuits = false;
bool bVerbose = false;
bool bExtraVerbose = false;

bool bRunTests = false;
bool bForcePAL = false;
bool bForceNTSC = false;

//* Framerate control:
const int FPS = 60;
const int DELAY = 100.0f / FPS;

//* Gamepad bits
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
Controller_t controllers[2];

static int emulation_thread(void *);
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
        SDL_CondSignal(frame_available_cond);
    } else {
        //if (!bRunTests && bExtraVerbose){
        //    puts("draw_frame(): dropping frame");
        //}
    }

    SDL_UnlockMutex(frame_lock);

    //* Wait to mantain framerate:
    frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < DELAY) {
        SDL_Delay((int)(DELAY - frameTime));
        //if (!bRunTests && bExtraVerbose){
        //    puts("draw_frame(): calling SDL_Delay()");
        //}
    }
}

static void audio_callback(void*, Uint8 *stream, int len) {
    assert(len >= 0);
    read_samples((int16_t*)stream , len/sizeof(int16_t));
}

void add_controller( int device_index)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type == Controller_t::k_Available) {
			controller.gamepad = SDL_GameControllerOpen(device_index);
			if (!controller.gamepad) {
				printf("Couldn't open gamepad: %s\n", SDL_GetError());
				return;
			}
			controller.joystick = SDL_GameControllerGetJoystick(controller.gamepad);
			printf("Opened gamepad %s at index %i\n", SDL_GameControllerName(controller.gamepad), i);
			controller.type = Controller_t::k_Gamepad;
			controller.instance_id = SDL_JoystickInstanceID(controller.joystick);
			return;
		}
	}
}

bool get_controller_index(SDL_JoystickID instance_id, int *controller_index)
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

void remove_controller(SDL_JoystickID instance_id)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type != Controller_t::k_Gamepad) {
			continue;
		}
		if (controller.instance_id != instance_id) {
			continue;
		}
		if (controller.type == Controller_t::k_Gamepad) {
			SDL_GameControllerClose(controller.gamepad);
            printf("Closed gamepad number %i\n", i);
		}
		controller.type = Controller_t::k_Available;
		return;
	}
}

void RunEmulation(){

    SDL_Thread *emu_thread;
    if (bExtraVerbose){
        puts("RunEmulation() called.");
    }
    
    exitFlag = false;
    
    if (!bShowGUI){
        running_state = true;
    }
    
    if(!(emu_thread = SDL_CreateThread(emulation_thread, "emulation", 0))) {
        printf("failed to create emulation thread: %s\n", SDL_GetError());
        exit(1);
    }

    while (!exitFlag)
    {
        sdl_thread();
        SDL_WaitThread(emu_thread, 0);
        if (exitFlag){
            exitFlag = false;
            break;
        }
    }
    running_state = false;
}

static int emulation_thread(void *){

    if(!bRunTests){
        if (bExtraVerbose){
            puts("emulation_thread() started, calling run().");
        }
        run();
    }else{
        if (bExtraVerbose){
            puts("emulation_thread() started, calling run_tests().");
        }
        run_tests();
    }
    if (bExtraVerbose){
        puts("emulation_thread() complete.");
    }
    return 0;
}

extern void process_events() {
    
    if(SDL_TryLockMutex(event_lock)){
        puts("process_events(): SDL_TryLockMutex failed!");
        return;
    };
    SDL_Event event;

    while (SDL_PollEvent(&event)) {

        //* Send events to emulation.
        switch(event.type)
        {
            case SDL_CONTROLLERBUTTONDOWN:
                int controller_index_down;
                if (!get_controller_index(event.cbutton.which, &controller_index_down)) {
                    break;
                }
                switch(event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_A:
                        set_button_state(controller_index_down,0);
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        set_button_state(controller_index_down,1);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        set_button_state(controller_index_down,4);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        set_button_state(controller_index_down,5);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        set_button_state(controller_index_down,6);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        set_button_state(controller_index_down,7);
                        break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        set_button_state(controller_index_down,2);
                        break;
                    case SDL_CONTROLLER_BUTTON_START:
                        set_button_state(controller_index_down,3);
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                        //* Change Saveslot -1 
                        GUI::DecreaseStateSlot();
                        break;
                    case SDL_CONTROLLER_BUTTON_Y:
                        //* Change Saveslot +1 
                        GUI::IncreaseStateSlot();
                        break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        //* Load State
                        if (bVerbose){
                            puts("user called load_state()");
                        }
                        GUI::LoadState();
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        //* Save State
                        if (bVerbose){
                            puts("user called save_state()");
                        }
                        GUI::SaveState();
                        break;
                    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                        puts("User Opened GUI");
                        GUI::PlaySound_UI(UI_SMB_PIPE);
                        GUI::PauseEmulation();
                        if (bRunTests){
                            exitFlag = true;
                        }
                        break;     
                    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                        //* Exit NESalizer!
                        puts("User quit!");
                        GUI::Shutdown();
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
                    case SDL_CONTROLLER_BUTTON_A:
                        clear_button_state(controller_index_up,0);
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        clear_button_state(controller_index_up,1);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        clear_button_state(controller_index_up,4);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        clear_button_state(controller_index_up,5);
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        clear_button_state(controller_index_up,6);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        clear_button_state(controller_index_up,7);
                        break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        clear_button_state(controller_index_up,2);
                        break;
                    case SDL_CONTROLLER_BUTTON_START:
                        clear_button_state(controller_index_up,3);
                        break;
                }
                break;
            case SDL_QUIT:
                GUI::Shutdown();
                break;
            case SDL_CONTROLLERDEVICEADDED:
                add_controller(event.cdevice.which);
                GUI::ShowTextOverlay("Gamepad Connected!");
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                remove_controller(event.cdevice.which);
                GUI::ShowTextOverlay("Gamepad Removed!");
                break;
        }

    }

    SDL_UnlockMutex(event_lock);
}

void sdl_thread() {

    int pitch;
    if (bExtraVerbose){
        puts("Entering sdl_thread().");
    }

    for(;;) {

        //* Wait for the emulation thread to signal that a frame has completed
        SDL_LockMutex(frame_lock);
        ready_to_draw_new_frame = true;

        while (!frame_available && !pending_sdl_thread_exit)
            SDL_CondWait(frame_available_cond, frame_lock);
        if (pending_sdl_thread_exit) {
            SDL_UnlockMutex(frame_lock);
            pending_sdl_thread_exit = false;
            if (bExtraVerbose){
                puts("quitting sdl_thread().");
            }
            return;
        }

        frame_available = ready_to_draw_new_frame = false;
        SDL_UnlockMutex(frame_lock);

        //* Check inputs.
        process_events();

        //* Still mid-run.. Stop Rendering
        if(bUserQuits){
            if (bExtraVerbose){
                puts("bUserQuits = 'true'. leaving sdl_thread().");
            }
            return;
        }
        
        //* Draw the new frame
        if(SDL_LockTexture(screen_tex, NULL, reinterpret_cast<void**>(&back_buffer), &pitch)){;
            printf("failed to update screen texture: %s", SDL_GetError());
            exit(1);
        }else{
            SDL_UnlockTexture(screen_tex);
        }

        //SDL_RenderClear(renderer);
        if(SDL_RenderCopy(renderer, screen_tex, NULL, NULL)) {
            printf("failed to copy rendered frame to render target: %s", SDL_GetError());
            exit(1);
        }
        //* Check if we need to show a message onscreen
        if (bShowOverlayText){
            unsigned int CurrentTickCount;
            CurrentTickCount = SDL_GetTicks();
            if(CurrentTickCount - OverlayTickCount < 2500){ //* 2.5secs
                int texW = 0;
                int texH = 0;
                //* Show the overlay
                SDL_QueryTexture(overlay_tex, NULL, NULL, &texW, &texH);
                SDL_Rect dstrect = { 10, 10, texW, texH };
                SDL_RenderCopy(renderer, overlay_tex, NULL, &dstrect);
            }else{
                //* Disable the overlay now 
                SDL_DestroyTexture(overlay_tex);
                bShowOverlayText=false;
            }
        } 
        SDL_RenderPresent(renderer);
    }
}

void exit_sdl_thread() {
    
    if (bExtraVerbose){
        puts("exit_sdl_thread() called.");
    }
    //* Wait for the emulation thread to signal that a frame has completed
    SDL_LockMutex(frame_lock);
    pending_sdl_thread_exit = true;
    SDL_CondSignal(frame_available_cond);
    SDL_UnlockMutex(frame_lock);
}

//* Initialization and de-initialization
void init_sdl() {

    puts("Initialising SDL.");
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0){
        printf("failed to initialize SDL: %s", SDL_GetError());
        exit(1);
    }
    
    SDL_ShowCursor(SDL_DISABLE);

    if (bVerbose){
        #ifdef USE_BLIP_ADD_DELTA_FAST
            puts("USE_BLIP_ADD_DELTA_FAST enabled!");
        #else
            puts("USE_BLIP_ADD_DELTA_FAST disabled.");
        #endif 
    }

    if(SDL_GameControllerAddMappingsFromFile("res/gamecontrollerdb.txt") == -1){
        printf("SDL_GameControllerAddMappingsFromFile(): %s", SDL_GetError());
    };

    if(!(screen = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 240, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL))) {
        printf("failed to create window: %s", SDL_GetError());
        exit(1);
    }

    #ifdef USE_VSYNC
        if(!(renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_PRESENTVSYNC))){
            printf("failed to create rendering context: %s", SDL_GetError());
            exit(1);
        }
    #else
        if(!(renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE))){
            printf("failed to create rendering context: %s", SDL_GetError());
            exit(1);
        }
    #endif


    //* Display some information about the renderer
    SDL_RendererInfo renderer_info;
    if (SDL_GetRendererInfo(renderer, &renderer_info))
        puts("Failed to get renderer information from SDL");
    else {
        if(bVerbose){
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
                puts("renderer: available texture formats:");
                printf("'");
            unsigned const n_texture_formats = min(16u, (unsigned)renderer_info.num_texture_formats);
            for (unsigned i = 0; i < n_texture_formats; ++i)
                printf(" %s", SDL_GetPixelFormatName(renderer_info.texture_formats[i]));
            printf("'\n");
        }
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    if(!(screen_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888 , SDL_TEXTUREACCESS_STREAMING , 256 , 240))) {
        printf("failed to create texture for screen: %s", SDL_GetError());
        exit(1);
    }

    back_buffer  = render_buffers[0];

    //* Audio
    SDL_AudioSpec want;
    SDL_AudioSpec got;

    want.freq     = sample_rate; 
    want.format   = AUDIO_S16SYS;  //* AUDIO_S16LSB in kevtroots switch port

    want.channels = 1;
    want.samples  = sdl_audio_buffer_size;
    want.callback = audio_callback;

    puts("Opening SDL Audio Device...");
    audio_device_id = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_ANY_CHANGE);
    
    if (bVerbose){
        printf("freq: %i, %i\n", want.freq, got.freq);
        printf("format: %i, %i\n", want.format, got.format);
        printf("channels: %i, %i\n", want.channels, got.channels);
        printf("samples: %i, %i\n", want.samples, got.samples);
    }

    //* SDL thread synchronization
    if(!(event_lock = SDL_CreateMutex())) {
        printf("failed to create event mutex: %s", SDL_GetError());
        exit(1);
    }
    if(!(frame_lock = SDL_CreateMutex())) {
        printf("failed to create frame mutex: %s", SDL_GetError());
        exit(1);
    }
    if(!(frame_available_cond = SDL_CreateCond())) {
        printf("failed to create frame condition variable: %s", SDL_GetError());
        exit(1);
    }
    
    GUI::init(screen,renderer);
}

void deinit_sdl() {

    if (bExtraVerbose){
        puts("Shutting down NESalizer!");
    }
    
    //* ImGUI Rom Dialog
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

    //* SDL Mutexs
    SDL_DestroyMutex(event_lock);
    SDL_DestroyMutex(frame_lock);
    SDL_DestroyCond(frame_available_cond);

    GUI::deinit();

    //* Sound
    SDL_CloseAudioDevice(audio_device_id); //* Prolly not needed, but play it safe

    //* Textures & Renderer
    SDL_DestroyTexture(screen_tex);
    SDL_DestroyRenderer(renderer); //* Also destroys the texture
    SDL_DestroyWindow(screen);

    //* Finally Quit SDL
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    SDL_Quit();
}
