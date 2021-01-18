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
static Controller_t controllers[2];

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
        //* printf("dropping frame\n");
    }

    SDL_UnlockMutex(frame_lock);
    //* Wait to mantain framerate:
    frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < DELAY) {
        SDL_Delay((int)(DELAY - frameTime));
    }
}

static void audio_callback(void*, Uint8 *stream, int len) {
    assert(len >= 0);
    read_samples((int16_t*)stream, len/sizeof(int16_t));
}

static void add_controller( int device_index)
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

static void remove_controller(SDL_JoystickID instance_id)
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

    int wheel = 0;
    int mouseX, mouseY;

    const int buttons = SDL_GetMouseState(&mouseX, &mouseY);
    ImGuiIO& io = ImGui::GetIO();

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
                    case  SDL_CONTROLLER_BUTTON_A:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,0);
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_B:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,1);
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_X:
                        if (!bShowGUI){
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
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_Y:
                        if (!bShowGUI){
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
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_BACK:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,2);
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_START:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,3);
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_UP:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,4);
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,5);
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,6);
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        if (!bShowGUI){
                            set_button_state(controller_index_down,7);
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        if (!bShowGUI){
                            //* Load State
                            printf("user called load_state()\n");
                            load_state();
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        if (!bShowGUI){
                            //* Save State
                            printf("user called save_state()\n");
                            save_state();
                        }
                        break;
                    case  SDL_CONTROLLER_BUTTON_LEFTSTICK:
                        if (!bShowGUI){
                            printf("User wants to select ROM!\n");
                            GUI::PlaySound_Pipe();
                            unload_rom();
                            end_emulation();
                            exit_sdl_thread();
                            GUI::stop_main_run();
                            bShowGUI=true;
                        }
                        break;     
                    case  SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                        //* Exit NESalizer!
                        printf("User quit!\n");
                        unload_rom();
                        end_emulation();
                        exit_sdl_thread();
                        GUI::stop_main_run();
                        deinit_sdl();
                        break;     
                    }
                break;
            case SDL_CONTROLLERBUTTONUP:
                if (!bShowGUI){
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
                    }
                }
                break;
            case SDL_MOUSEWHEEL:
				wheel = event.wheel.y;
				break;
            case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				{
					io.DisplaySize.x = static_cast<float>(event.window.data1);
					io.DisplaySize.y = static_cast<float>(event.window.data2);
				}
				break;
        }
    }

    if (bShowGUI){
        io.DeltaTime = 1.0f / 60.0f;
        io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
        io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
        io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
        io.MouseWheel = static_cast<float>(wheel);
        ImGuiSDL::ProcessEvent(&event);
    }

    SDL_UnlockMutex(event_lock);
}

void sdl_thread() {
    int texW = 0; //* for the overlay
    int texH = 0;
    SDL_UnlockMutex(frame_lock);
    for(;;) {
        //* Wait for the emulation thread to signal that a frame has completed
        SDL_LockMutex(frame_lock);         //* KevRoot's port commented this out
        ready_to_draw_new_frame = true;
        while (!frame_available && !pending_sdl_thread_exit)
            SDL_CondWait(frame_available_cond, frame_lock);
        if (pending_sdl_thread_exit) {
            SDL_UnlockMutex(frame_lock);
            pending_sdl_thread_exit = false;
            return;
        }
        frame_available = ready_to_draw_new_frame = false;
        SDL_UnlockMutex(frame_lock);        //* KevRoot's port commented this out
        process_events();
        
        //* Draw the new frame
        if(SDL_UpdateTexture(screen_tex, 0, front_buffer, 256*sizeof(Uint32))) {
            printf("failed to update screen texture: %s", SDL_GetError());
            exit(1);
        }
        if(SDL_RenderCopy(renderer, screen_tex, 0, 0)) {
            printf("failed to copy rendered frame to render target: %s", SDL_GetError());
            exit(1);
        }
        //* Check if we need to show a message onscreen
        if (bShowOverlayText){
            unsigned int CurrentTickCount;
            CurrentTickCount = SDL_GetTicks();
            if(CurrentTickCount - OverlayTickCount < 2500) //* 2.5secs
            {
                //* Show the overlay
                SDL_Surface * overlay_surface = TTF_RenderText_Blended(overlay_font, TextOverlayMSG.c_str(), overlay_color);
                SDL_Texture * overlay_texture = SDL_CreateTextureFromSurface(renderer, overlay_surface);
                SDL_QueryTexture(overlay_texture, NULL, NULL, &texW, &texH);
                SDL_Rect dstrect = { 10, 10, texW, texH };
                SDL_RenderCopy(renderer, overlay_texture, NULL, &dstrect);
                SDL_DestroyTexture(overlay_texture);
                SDL_FreeSurface(overlay_surface);
            }else
            {
                //* Disable the overlay now 
                bShowOverlayText=false;
            }
            
            
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

//* Initialization and de-initialization
void init_sdl() {

    printf("Initialising SDL.\n");
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("failed to initialize SDL: %s", SDL_GetError());
        exit(1);
    }

    if(SDL_GameControllerAddMappingsFromFile("res/gamecontrollerdb.txt") == -1){
        printf("SDL_GameControllerAddMappingsFromFile(): %s", SDL_GetError());
    };

    printf("Creating SDL Window.\n");
    if(!(screen = SDL_CreateWindow(NULL,SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED , 256 , 240 , SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL))) 
    {
        printf("failed to create window: %s", SDL_GetError());
        exit(1);
    }

    if( TTF_Init() == -1 )
    {
        printf("failed to init SDL_TTF: %s", SDL_GetError());
        exit(1);  
    }

    printf("Creating SDL Renderer.\n");
    if(!(renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_PRESENTVSYNC))) 
    {
        printf("failed to create rendering context: %s", SDL_GetError());
        exit(1);
    }

    //* Display some information about the renderer
    SDL_RendererInfo renderer_info;
    printf("Getting Renderer Info\n");
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

    printf("SDL_SetHint to 'nearest'\n");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    printf("Creating SDL Texture\n");              
    if(!(screen_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888 , SDL_TEXTUREACCESS_STREAMING , 256 , 240))) 
    {
        printf("failed to create texture for screen: %s", SDL_GetError());
        exit(1);
    }

    static Uint32 render_buffers[2][240*256];
    back_buffer  = render_buffers[0];
    front_buffer = render_buffers[1];

    //* Audio
    SDL_AudioSpec want;
    SDL_AudioSpec got;

    want.freq     = sample_rate; 
    want.format   = AUDIO_S16LSB;  //* AUDIO_S16SYS in original - AUDIO_S16LSB in kevtroots switch port
    want.channels = 1;
    want.samples  = sdl_audio_buffer_size;
    want.callback = audio_callback;

    printf("Opening SDL Audio Device\n");
    audio_device_id = SDL_OpenAudioDevice(0, 0, &want, &got, SDL_AUDIO_ALLOW_ANY_CHANGE);
    
    printf("freq: %i, %i\n", want.freq, got.freq);
    printf("format: %i, %i\n", want.format, got.format);
    printf("channels: %i, %i\n", want.channels, got.channels);
    printf("samples: %i, %i\n", want.samples, got.samples);
    
    //* SDL thread synchronization
    printf("Creating 'event_lock' Mutex\n");
    if(!(event_lock = SDL_CreateMutex())) {
        printf("failed to create event mutex: %s", SDL_GetError());
        exit(1);
    }
    printf("Creating 'frame_lock' Mutex\n");
    if(!(frame_lock = SDL_CreateMutex())) {
        printf("failed to create frame mutex: %s", SDL_GetError());
        exit(1);
    }
   
    printf("Setting 'frame_available_cond' condition.\n");
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

    //* Block until a ROM is selected
    SDL_ShowCursor(SDL_DISABLE);
    GUI::init(screen, renderer);
}

void deinit_sdl() {

    puts("Shutting down NESalizer!");
    puts("-------------------------------------------------------");
    
    //* ImGUI Rom Dialog
    ImGuiSDL::Deinitialize();
    puts("ImGuiSDL::Deinitialize()");
	ImGui::DestroyContext();
    puts("ImGui::DestroyContext()");

    //* SDL Mutexs
    SDL_DestroyMutex(event_lock);
    puts("SDL_DestroyMutex(event_lock)");
    SDL_DestroyMutex(frame_lock);
    puts("SDL_DestroyMutex(frame_lock)");
    SDL_DestroyCond(frame_available_cond);
    puts("SDL_DestroyCond(frame_available_cond)");

    //* GUI Overlay
    TTF_CloseFont(overlay_font);
    puts("TTF_CloseFont(overlay_font)");
    TTF_Quit();
    puts("TTF_Quit()");

    //* GUI Sound Effects
    Mix_Quit();
    puts("Mix_Quit()");

    //* GUI Wallpaper
    IMG_Quit();
    puts("IMG_Quit()");

    //* Sound
    SDL_CloseAudioDevice(audio_device_id); //* Prolly not needed, but play it safe
    puts("SDL_CloseAudioDevice(audio_device_id)");

    //* Textures & Renderer
    SDL_DestroyTexture(screen_tex);
    puts("SDL_DestroyTexture(screen_tex)");

    SDL_DestroyRenderer(renderer); //* Also destroys the texture
    puts("SDL_DestroyRenderer(renderer)");

    SDL_DestroyWindow(screen);
    puts("SDL_DestroyWindow(screen)");

    //* Finally Quit SDL
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    puts("SDL_QuitSubSystem(SDL_INIT_EVERYTHING)");
    SDL_Quit();
    puts("SDL_Quit()");
    puts("-------------------------------------------------------");
}
