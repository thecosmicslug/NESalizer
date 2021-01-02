
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <string>
#include <csignal>
#include <iostream>

#include "common.h"
#include "sdl_backend.h"
#include "save_states.h"
#include "cpu.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_frontend.h"

using std::string;

//* Default false, we assume a ROM will be supplied, GUI if not.
bool bShowGUI=false;
bool bShowOverlayText=false;

std::string TextOverlayMSG;
unsigned int OverlayTickCount;
char *statename;
int statenum=0;

using std::string;

void replaceExt(string& s, const string& newExt) {

   string::size_type i = s.rfind('.', s.length());

   if (i != string::npos) {
      s.replace(i+1, newExt.length(), newExt);
   }
}

namespace GUI
{

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *screen_tex;

static int emulation_thread(void*);

SDL_Texture *background;

bool pause = true;
bool exitFlag = false;

void SetROMStateFilename()
{
    //* Strip Path , Add State folder , Replace extension
    string filename = basename(fname);
    string path = "states/" + filename;
    replaceExt(path, "state");

    //* Add statenumber
    path = path + std::to_string(statenum);

    //* Convert back to char-array
    statename = new char [path.length()+1];
    strcpy (statename, path.c_str());
    printf("Setting Savestate to '%s'\n", statename);
}

void ShowTextOverlay(std::string MSG)
{
    TextOverlayMSG=MSG;
    printf("Showing Text Overlay MSG '%s'\n", MSG);
    bShowOverlayText=true;
    OverlayTickCount = SDL_GetTicks();
}

bool saveScreenshot(const std::string &file) {
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

void stop_main_run()
{
    exitFlag = true;
}

void main_run()
{
    SDL_Thread *emu_thread;

    //* Get initial frame lock until ROM is chosen.
    //*SDL_LockMutex(frame_lock);                //* Commented out in Kevroots switch port
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

    //* Setup ImGUI Backend for our interface
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        printf("IMG_Init(): %s\n", IMG_GetError());
        exit(1);
    }

    SDL_Surface *backSurface = IMG_Load("res/wallpaper.png");
    if(!backSurface) {
        printf("IMG_Load: %s\n", IMG_GetError());
    }
    else
    {
        background = SDL_CreateTextureFromSurface(renderer, backSurface);
        SDL_FreeSurface(backSurface);
        if (!background)
        {
            printf("SDL_CreateTextureFromSurface(): %s\n",  SDL_GetError());
        }
    }
    
    puts("Setting up ImGui::CreateContext()\n");
	ImGui::CreateContext();

	//** Enable Gamepad Controls
	ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; 
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.IniFilename = nullptr; 
    
    puts("Initialising  ImGuiSDL::Initialize\n");
	ImGuiSDL::Initialize(renderer,window, 256 , 240);
	ImGui::StyleColorsDark();
}

//* Render ImGUI File Dialog
void render()
{
    static ImGuiFs::Dialog dlg;
    const char* chosenPath;

    SDL_RenderClear(renderer);
    if(SDL_RenderCopy(renderer, background, 0, 0)) {
        printf("failed to copy GUI background to render target: %s", SDL_GetError());
    }

    ImGuiSDL::NewFrame(window);
    ImGui::Begin("NESalizer",NULL,ImGuiWindowFlags_NoSavedSettings);
    chosenPath = dlg.chooseFileDialog(bShowGUI,"./roms/",".nes", "Choose a ROM.");

    ImGui::End();
    ImGui::Render();
    ImGuiSDL::Render(ImGui::GetDrawData());

    if (strlen(dlg.getChosenPath())>0) {
        if(load_rom(chosenPath)){
            SetROMStateFilename();
            std::string tmpstr = "ROM  '";
            tmpstr  += basename(fname);
            tmpstr  += "'  Loaded!";
            ShowTextOverlay(tmpstr); 
            bShowGUI=false;
        };
    }
    
    SDL_RenderPresent(renderer);

}

void unload_rom()
{
    unload_rom();
}

/* Play/stop the game */
void toggle_pause()
{
    pause = !pause;

    //* Set CPU emulation to paused
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

} //* namespace GUI