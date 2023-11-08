#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "SDL_mixer.h"
#include <SDL_ttf.h>

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
#include "imgui_impl_sdl.h"

SDL_Window   *screen;
SDL_Renderer *renderer;
SDL_Texture  *screen_tex;

Uint32 *back_buffer __attribute__((aligned(32)));

Uint32 render_buffers[2][240*256] __attribute__((aligned(32)));
int pitch;

static bool ready_to_draw_new_frame;
static bool frame_available;
static bool pending_sdl_thread_exit;

bool bUserQuits;

static SDL_mutex *frame_lock;
static SDL_mutex *event_lock;
static SDL_cond  *frame_available_cond;
static SDL_AudioDeviceID audio_device_id;

//* Framerate control:
const int FPS = 60;
const int DELAY = 100.0f / FPS;

//* Onscreen Text Overlay
TTF_Font *overlay_font;
SDL_Color overlay_color = {255,255,0}; //* YELLOW

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
        //puts("draw_frame(): dropping frame");
    }

    SDL_UnlockMutex(frame_lock);

    //* Wait to mantain framerate:
    frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < DELAY) {
        //puts("draw_frame(): calling SDL_Delay()");
        SDL_Delay((int)(DELAY - frameTime));
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
			printf("Opened game controller %s at index %i\n", SDL_GameControllerName(controller.gamepad), i);
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
            printf("Closed game controller number %i\n", i);
		}
		controller.type = Controller_t::k_Available;
		return;
	}
}

extern void process_events() {
    
    SDL_LockMutex(event_lock);
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
                    {
                        //* Change Saveslot Minus 1 
                        if (statenum == 0){
                            statenum = 9;
                        }else{
                            statenum = statenum - 1;
                        }
                        std::string tmpstr = "Save-State Slot '";
                        tmpstr += std::to_string(statenum);
                        tmpstr += "' Activated.";
                        GUI::SetROMStateFilename();
                        GUI::ShowTextOverlay(tmpstr);
                        GUI::PlaySound_Coin();
                        break;
                    }
                    case SDL_CONTROLLER_BUTTON_Y:
                    {
                        //* Change Saveslot Plus 1 
                        if (statenum == 9){
                            statenum = 0;
                        }else{
                            statenum = statenum + 1;
                        }
                        std::string tmpstr = "Save-State Slot '";
                        tmpstr += std::to_string(statenum);
                        tmpstr += "' Activated.";
                        GUI::SetROMStateFilename();
                        GUI::ShowTextOverlay(tmpstr);
                        GUI::PlaySound_Coin();
                        break;
                    }
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        //* Load State
                        puts("user called load_state()");
                        load_state();
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        //* Save State
                        puts("user called save_state()");
                        save_state();
                        break;
                    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                        puts("User wants to select ROM!");
                        GUI::PlaySound_Pipe();
                        //unload_rom();
                        end_emulation();
                        exit_sdl_thread();
                        GUI::stop_main_run();
                        bShowGUI = true;
                        break;     
                    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                        //* Exit NESalizer!
                        puts("User quit!");
                        unload_rom();
                        end_emulation();
                        exit_sdl_thread();
                        GUI::stop_main_run();
                        bUserQuits = true;
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
                unload_rom();
                end_emulation();
                exit_sdl_thread();
                if (bRunTests){
                    end_testing = true;
                }
                bUserQuits = true;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                add_controller(event.cdevice.which);
                GUI::ShowTextOverlay("Controller Connected!");
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                remove_controller(event.cdevice.which);
                GUI::ShowTextOverlay("Controller Removed!");
                break;
        }

    }

    SDL_UnlockMutex(event_lock);
}


extern void process_gui_events() {
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        switch(event.type)
        {
            case SDL_QUIT:
                unload_rom();
                end_emulation();
                exit_sdl_thread();
                if (bRunTests){
                    end_testing = true;
                }
                bUserQuits = true;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                add_controller(event.cdevice.which);
                GUI::ShowTextOverlay("Controller Connected!");
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                remove_controller(event.cdevice.which);
                GUI::ShowTextOverlay("Controller Removed!");
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                int controller_index_down;
                if (!get_controller_index(event.cbutton.which, &controller_index_down)) {
                    break;
                }
                switch(event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                        //* Exit NESalizer!
                        puts("User quit!");
                        unload_rom();
                        end_emulation();
                        exit_sdl_thread();
                        GUI::stop_main_run();
                        deinit_sdl();
                        bUserQuits = true;
                        break;     
                }
                break;
        }
        ImGui_ImplSDL2_ProcessEvent(&event);
    }

}

void sdl_thread() {

    for(;;) {

        //* Wait for the emulation thread to signal that a frame has completed
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

        //* Check inputs.
        process_events();

        //* Still mid-run.. Stop Rendering
        if(bUserQuits){
            return;
        }
        
        //* Draw the new frame
        if(SDL_LockTexture(screen_tex, NULL, reinterpret_cast<void**>(&back_buffer), &pitch)){;
            printf("failed to update screen texture: %s", SDL_GetError());
            exit(1);
        }else{
            SDL_UnlockTexture(screen_tex);
        }
        SDL_RenderClear(renderer);
        if(SDL_RenderCopy(renderer, screen_tex, NULL, NULL)) {
            printf("failed to copy rendered frame to render target: %s", SDL_GetError());
            exit(1);
        }
        //* Check if we need to show a message onscreen
        if (bShowOverlayText){
            int texW = 0; //* for the overlay
            int texH = 0;
            unsigned int CurrentTickCount;
            CurrentTickCount = SDL_GetTicks();
            if(CurrentTickCount - OverlayTickCount < 2500){ //* 2.5secs
                //* Show the overlay
                SDL_Surface * overlay_surface = TTF_RenderText_Blended(overlay_font, TextOverlayMSG.c_str(), overlay_color);
                SDL_Texture * overlay_texture = SDL_CreateTextureFromSurface(renderer, overlay_surface);
                SDL_QueryTexture(overlay_texture, NULL, NULL, &texW, &texH);
                SDL_Rect dstrect = { 10, 10, texW, texH };
                SDL_RenderCopy(renderer, overlay_texture, NULL, &dstrect);
                SDL_DestroyTexture(overlay_texture);
                SDL_FreeSurface(overlay_surface);
            }else{
                //* Disable the overlay now 
                bShowOverlayText=false;
            }
        } 
        SDL_RenderPresent(renderer);
    }
}

void exit_sdl_thread() {
    SDL_LockMutex(frame_lock);
    pending_sdl_thread_exit = true;
    SDL_CondSignal(frame_available_cond);
    SDL_UnlockMutex(frame_lock);
}

//* Initialization and de-initialization
void init_sdl() {

    puts("Initialising SDL.");
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("failed to initialize SDL: %s", SDL_GetError());
        exit(1);
    }

     SDL_ShowCursor(SDL_DISABLE);

    if(SDL_GameControllerAddMappingsFromFile("res/gamecontrollerdb.txt") == -1){
        printf("SDL_GameControllerAddMappingsFromFile(): %s", SDL_GetError());
    };

    if(!(screen = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 240, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL))) 
    {
        printf("failed to create window: %s", SDL_GetError());
        exit(1);
    }

    if( TTF_Init() == -1 )
    {
        printf("failed to init SDL_TTF: %s", SDL_GetError());
        exit(1);  
    }

    if(!(renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE ))) //// | SDL_RENDERER_PRESENTVSYNC))) 
    {
        printf("failed to create rendering context: %s", SDL_GetError());
        exit(1);
    }

    //* Display some information about the renderer
    SDL_RendererInfo renderer_info;
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
            puts("renderer: available texture formats:");
        unsigned const n_texture_formats = min(16u, (unsigned)renderer_info.num_texture_formats);
        for (unsigned i = 0; i < n_texture_formats; ++i)
            printf(" %s", SDL_GetPixelFormatName(renderer_info.texture_formats[i]));
        putchar('\n');
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    if(!(screen_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888 , SDL_TEXTUREACCESS_STREAMING , 256 , 240))) 
    {
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

    puts("Opening SDL Audio Device");
    audio_device_id = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_ANY_CHANGE);
    
    printf("freq: %i, %i\n", want.freq, got.freq);
    printf("format: %i, %i\n", want.format, got.format);
    printf("channels: %i, %i\n", want.channels, got.channels);
    printf("samples: %i, %i\n", want.samples, got.samples);
    
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
    
    //* Load a nice retro font,
    //* https://www.fontspace.com/diary-of-an-8-bit-mage-font-f28455
    overlay_font = TTF_OpenFont("res/DiaryOfAn8BitMage.ttf", 30);
    if(!overlay_font) {
        printf("TTF_OpenFont: %s\n", TTF_GetError());
    }

    GUI::init(screen,renderer);
}

void deinit_sdl() {

    puts("Shutting down NESalizer!");

    //* ImGUI Rom Dialog
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

    //* SDL Mutexs
    SDL_DestroyMutex(event_lock);
    SDL_DestroyMutex(frame_lock);
    SDL_DestroyCond(frame_available_cond);

    //* GUI Overlay
    TTF_CloseFont(overlay_font);
    TTF_Quit();

    //* GUI Sound Effects
    Mix_Quit();

    //* GUI Wallpaper
    IMG_Quit();

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
