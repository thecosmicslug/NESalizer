
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "SDL_mixer.h"

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
#include "test.h"

using std::string;

bool bRunTests=false;
char *testfilename;

bool bForcePAL=false;
bool bForceNTSC=false;
bool bShowGUI=true;
bool bShowOverlayText=false;

std::string TextOverlayMSG;
unsigned int OverlayTickCount;
char *statename;
char *savename;
int statenum=0;

#define NUM_WAVEFORMS 3
const char* _waveFileNames[] =
{
"res/smb_bump.wav", //0
"res/smb_coin.wav", //1
"res/smb_pipe.wav", //2
};

Mix_Chunk* _sample[3];

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

static int emulation_thread(void*);

SDL_Texture *background;

bool pause = true;
bool exitFlag = false;

void PlaySound_Pipe(){
    //* Mario Pipe Effect - Showing GUI
    Mix_PlayChannel(-1, _sample[2], 0);
}

void PlaySound_Coin(){
    //* Mario Coin Effect - Positive Result
    Mix_PlayChannel(-1, _sample[1], 0);
}

void PlaySound_Bump(){
    //* Mario Bumping Blocks - Negative Result
    Mix_PlayChannel(-1, _sample[0], 0);
}

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

void SetSRAMFilename()
{
    //* Strip Path , Add State folder , Replace extension
    string filename = basename(fname);
    string path = "saves/" + filename;
    replaceExt(path, "sav");

    //* Convert back to char-array
    savename = new char [path.length()+1];
    strcpy (savename, path.c_str());
    printf("Setting SRAM file to '%s'\n", savename);
}

void ShowTextOverlay(std::string MSG)
{
    //* Change Message String, Update TickCount
    TextOverlayMSG=MSG;
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

    //* Init SDL_Mixer for our GUI Sounds
    memset(_sample, 0, sizeof(Mix_Chunk*) * 2);
    int result = Mix_OpenAudio(sample_rate, AUDIO_S16LSB, 2, 512);
    if( result < 0 )
    {
        puts("Unable to open audio:");
    }
    result = Mix_AllocateChannels(4);
    if( result < 0 )
    {
        puts("Unable to allocate mixing channels:");
    }

    //* Load WAVs for later
    for( int i = 0; i < NUM_WAVEFORMS; i++ )
    {
        _sample[i] = Mix_LoadWAV(_waveFileNames[i]);
        if( _sample[i] == NULL )
        {
            puts("Unable to load wave file");
        }
    }
    
    puts("Setting up ImGui::CreateContext()");
	ImGui::CreateContext();

	//** Enable Gamepad Controls
	ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; 
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.IniFilename = nullptr; 
    
    puts("Initialising  ImGuiSDL::Initialize");
	//ImGuiSDL::Initialize(renderer,window, 256 , 240);
    ImGui_ImplSDL2_InitForSDLRenderer(window,renderer);
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

    //ImGuiSDL::NewFrame(window);
    ImGui_ImplSDL2_NewFrame();
    ImGui::Begin("NESalizer",NULL,ImGuiWindowFlags_NoSavedSettings);
    chosenPath = dlg.chooseFileDialog(bShowGUI,"./roms/",".nes", "Choose a ROM.");

    ImGui::End();
    ImGui::Render();
    ImGui::GetDrawData();

    //ImGuiSDL::Render(ImGui::GetDrawData());

    if (strlen(dlg.getChosenPath())>0) {
        if(load_rom(chosenPath)){
            SetROMStateFilename();
            std::string tmpstr = "ROM  '";
            tmpstr  += basename(fname);
            tmpstr  += "'  Loaded!";
            ShowTextOverlay(tmpstr);
            GUI::PlaySound_Coin();
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
    if(!bRunTests){
        run();
    }else{
        run_tests();
    }
    return 0;
}

} //* namespace GUI