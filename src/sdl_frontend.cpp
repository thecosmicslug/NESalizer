
#include <string>
#include <csignal>

#include "common.h"
#include "sdl_backend.h"
#include "save_states.h"
#include "cpu.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_frontend.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

// Default false, we assume a ROM will be supplied, GUI if not.
bool bShowGUI=false;

namespace GUI
{

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *screen_tex;

static int emulation_thread(void*);

SDL_Texture *background;

bool pause = true;
bool exitFlag = false;

void unload_rom()
{
    unload_rom();
}

void stop_main_run()
{
    exitFlag = true;
}

void main_run()
{
    SDL_Thread *emu_thread;

    // Get initial frame lock until ROM is chosen.
    //SDL_LockMutex(frame_lock);                // Commented out in Kevroots switch port
    printf("creating emulation thread, running_state & exitFlag set\n");
    exitFlag = false;
    running_state = true;
    if(!(emu_thread = SDL_CreateThread(emulation_thread, "emulation", 0))) {
        printf("failed to create emulation thread: %s\n", SDL_GetError());
        exit(1);
    }

    while (true)
    {
        printf("In main_run loop\n");
        sdl_thread();
        SDL_WaitThread(emu_thread, 0);
        if (exitFlag)
        {
            exitFlag = false;
            return;
        }
    }
}

void init(SDL_Window *scr, SDL_Renderer *rend)
{
    renderer = rend;
    window = scr;
    puts("Setting up ImGui\n");
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        puts("Error Setting up ImGui::init() SDL_CreateTexture()\n");
        exit(1);
    }

    puts("Setting up ImGUI background texture\n");
    SDL_Surface *backSurface = IMG_Load("bgwallpaper.png");
    background = SDL_CreateTextureFromSurface(renderer, backSurface);
    SDL_FreeSurface(backSurface);
    if (!background)
    {
        puts("Error Setting up ImGui::init() SDL_CreateTextureFromSurface()\n");
        exit(1);
    }

    // Setup Dear ImGui
    puts("Setting up ImGui::CreateContext()\n");
	ImGui::CreateContext();

	// Enable Gamepad Controls
	ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; 
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    
    puts("Initialising  ImGuiSDL::Initialize\n");
	ImGuiSDL::Initialize(renderer,window, 256 , 240);
	ImGui::StyleColorsDark();
}

// Render ImGUI File Dialog
void render()
{
    static ImGuiFs::Dialog dlg;
    const char* chosenPath;

    SDL_RenderClear(renderer);
    if(SDL_RenderCopy(renderer, background, 0, 0)) {
        printf("failed to copy GUI background to render target: %s", SDL_GetError());
    }

    ImGuiSDL::NewFrame(window);
    ImGui::Begin("Load NES Rom...",NULL,ImGuiWindowFlags_NoSavedSettings);
    
    //chosenPath = dlg.chooseFileDialog(bShowGUI,NULL,".nes");
    chosenPath = dlg.chooseFileDialog(bShowGUI,NULL,".nes", "Choose a NES ROM...");

    ImGui::End();
    ImGui::Render();
    ImGuiSDL::Render(ImGui::GetDrawData());

    SDL_RenderPresent(renderer);

    if (strlen(dlg.getChosenPath())>0) {
        printf("Loading ROM: %s\n", chosenPath);
        load_rom(chosenPath);
        bShowGUI=false;
    }
    
}

/* Play/stop the game */
void toggle_pause()
{
    pause = !pause;

    // Set CPU emulation to paused
    if (pause)
    {
        printf("toggle_pause() - Paused\n");
        SDL_LockMutex(frame_lock);
        running_state = false;
        
    }
    else
    {
        printf("toggle_pause() - Resume\n");
        running_state = true;
        SDL_UnlockMutex(frame_lock);
    }
}


static int emulation_thread(void *)
{
    run();
    return 0;
}

} // namespace GUI