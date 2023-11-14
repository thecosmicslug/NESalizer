
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "SDL_mixer.h"
#include <SDL_ttf.h>

#include "common.h"
#include "save_states.h"
#include "cpu.h"
#include "mapper.h"
#include "rom.h"
#include "test.h"
#include "sdl_backend.h"
#include "sdl_frontend.h"

bool bShowOverlayText = false;
bool bShowGUI = true;

std::string TextOverlayMSG;
unsigned int OverlayTickCount;

//* Currently loaded ROM
const char *loaded_rom_name;

//* Save-states
char *statename;
int statenum=0;

//* UI Sounds
#define NUM_WAVEFORMS 3
const char* _waveFileNames[] =
{
    "res/smb_bump.wav", //0
    "res/smb_coin.wav", //1
    "res/smb_pipe.wav", //2
};

//* Onscreen Text Overlay
TTF_Font *overlay_font;
SDL_Color overlay_color = {255,255,0}; //* YELLOW

Mix_Chunk* _sample[3];

SDL_Texture *background;
SDL_Renderer *GUIrenderer;

using std::string;

namespace GUI
{

bool PlaySound_UI(UISound effect){
    Mix_PlayChannel(-1, _sample[effect], 0);
    return true;
}

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

void SetROMStateFilename(){
    //* Strip Path , Add State folder , Replace extension
    string filename = basename(loaded_rom_name);
    string path = "states/" + filename;
    replaceExt(path, "state");

    //* Add statenumber
    path = path + std::to_string(statenum);

    //* Convert back to char-array
    statename = new char [path.length()+1];
    strcpy (statename, path.c_str());
    if (bVerbose){
        printf("Setting Savestate to '%s'\n", statename);
    }

}

void SaveState(){

    //* Save to current state-slot
    if (save_state(statename)){

        std::string tmpstr = "State  '";
        tmpstr += basename(statename);
        tmpstr  += "'  Saved!";
        ShowTextOverlay(tmpstr);
        //PlaySound_Coin();
        GUI::PlaySound_UI(UI_SMB_COIN);

    }else{

        std::string tmpstr = "Failed to save-state '";
        tmpstr += basename(statename);
        tmpstr += "'";
        ShowTextOverlay(tmpstr);
        //PlaySound_Bump();
        GUI::PlaySound_UI(UI_SMB_BUMP);
    };

}

void LoadState(){

    //* Load Savestate from file
    if(load_state(statename)){

        std::string tmpstr = "State  '";
        tmpstr += basename(statename);
        tmpstr += "'  Loaded!";
        //PlaySound_Coin();
        GUI::PlaySound_UI(UI_SMB_COIN);
        ShowTextOverlay(tmpstr);

    }else{

        std::string tmpstr = "'";
        tmpstr += basename(statename);
        tmpstr += "'  Not Found!";
        ShowTextOverlay(tmpstr);
        //PlaySound_Bump();
        GUI::PlaySound_UI(UI_SMB_BUMP);
    };
}

void IncreaseStateSlot(){
    
    //* Change Saveslot +1 
    if (statenum == 9){
        statenum = 0;
    }else{
        statenum = statenum + 1;
    }

    std::string tmpstr = "Save-State Slot '";
    tmpstr += std::to_string(statenum);
    tmpstr += "' Activated.";
    SetROMStateFilename();
    ShowTextOverlay(tmpstr);
    //PlaySound_Coin();
    GUI::PlaySound_UI(UI_SMB_COIN);
}

void DecreaseStateSlot(){
    //* Change Saveslot -1 
    if (statenum == 0){
        statenum = 9;
    }else{
        statenum = statenum - 1;
    }
    std::string tmpstr = "Save-State Slot '";
    tmpstr += std::to_string(statenum);
    tmpstr += "' Activated.";
    SetROMStateFilename();
    ShowTextOverlay(tmpstr);
    //PlaySound_Coin();
    GUI::PlaySound_UI(UI_SMB_COIN);
}

void ShowTextOverlay(std::string MSG){

    if (overlay_tex){
        //* Destroy texture from last time before we re-use it
        SDL_DestroyTexture(overlay_tex);
    }

    //* Change Message String, Update TickCount
    TextOverlayMSG=MSG;
    SDL_Surface * overlay_surface = TTF_RenderUTF8_Solid(overlay_font, TextOverlayMSG.c_str(), overlay_color);
    overlay_tex = SDL_CreateTextureFromSurface(GUIrenderer, overlay_surface);
    SDL_FreeSurface(overlay_surface);

    //* Activate
    bShowOverlayText=true;
    OverlayTickCount = SDL_GetTicks();

}

bool saveScreenshot(const std::string &file) {
  SDL_Rect _viewport;
  SDL_Surface *_surface = NULL;
  SDL_RenderGetViewport(GUIrenderer, &_viewport);
  _surface = SDL_CreateRGBSurface( 0, _viewport.w, _viewport.h, 32, 0, 0, 0, 0 );
  if ( _surface == NULL ) {
    std::cout << "Cannot create SDL_Surface: " << SDL_GetError() << std::endl;
    return false;
   }
  if ( SDL_RenderReadPixels(GUIrenderer, NULL, _surface->format->format, _surface->pixels, _surface->pitch ) != 0 ) {
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

void StopEmulation(){

    if (bExtraVerbose){
        puts("calling end_emulation().");
    }
    end_emulation();

    if (bExtraVerbose){
        puts("calling exit_sdl_thread().");
    }
    exit_sdl_thread();
    
    if (bExtraVerbose){
        puts("StopEmulation(): exitFlag = 'true'.");
    }
    exitFlag = true;

}

//* Play/stop the emulation. 
void TogglePauseEmulation(){

    if (running_state){
        running_state = false;
        bShowGUI = true;
    }else{
        bShowGUI = false;
        running_state = true;
    }

}

bool LoadROM(char const *romfile){
    //* Try Loading the supplied ROM
    if(load_rom(romfile)){
        //* Update loaded ROM filename
        loaded_rom_name = romfile;
        //* Set savestates up
        SetROMStateFilename();
        std::string tmpstr = "ROM '";
        tmpstr  += basename(loaded_rom_name);
        tmpstr  += "' Loaded!";
        //* Show Overlay message.
        ShowTextOverlay(tmpstr);
        return true;
    }else{
        return false;
    };
}

void init(SDL_Window* scr, SDL_Renderer* rend){

    GUIrenderer = rend;

    if( TTF_Init() == -1 )
    {
        printf("failed to init SDL_TTF: %s", SDL_GetError());
        exit(1);  
    }

    //* Load a nice retro font,
    //* https://www.fontspace.com/diary-of-an-8-bit-mage-font-f28455
    overlay_font = TTF_OpenFont("res/DiaryOfAn8BitMage.ttf", 30);
    if(!overlay_font) {
        printf("TTF_OpenFont: %s\n", TTF_GetError());
    }

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
        background = SDL_CreateTextureFromSurface(GUIrenderer, backSurface);
        SDL_FreeSurface(backSurface);
        if (!background){
            printf("SDL_CreateTextureFromSurface(): %s\n",  SDL_GetError());
        }
    }

    //* Init SDL_Mixer for our GUI Sounds
    memset(_sample, 0, sizeof(Mix_Chunk*) * 2);
    int result = Mix_OpenAudio(sample_rate, AUDIO_S16SYS, 1, 1024);
    if( result < 0 ){
        printf("Mix_OpenAudio: Unable to open audio: %s\n",Mix_GetError());
    }
    result = Mix_AllocateChannels(4);
    if( result < 0 ){
        printf("Mix_AllocateChannels: Unable to allocate mixing channels: %s\n",Mix_GetError());
    }

    //* Load WAVs for later
    for( int i = 0; i < NUM_WAVEFORMS; i++ ){
        _sample[i] = Mix_LoadWAV(_waveFileNames[i]);
        if( _sample[i] == NULL ){
            printf("Mix_LoadWAV: Unable to load '.wav' file  %s\n",Mix_GetError());
        }
    }
    
    if (bVerbose){
        puts("Setting up ImGUI with Gamepad support..");
    }
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    //** Enable Gamepad Controls
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;   // Hide Mouse Cursor 
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(scr, GUIrenderer);
    ImGui_ImplSDLRenderer_Init(GUIrenderer);
}

//* Unload GUI bits
void deinit(){

    //* GUI Overlay
    if (!overlay_tex){
        SDL_DestroyTexture(overlay_tex);
    }
    TTF_CloseFont(overlay_font);
    TTF_Quit();

    //* GUI Sound Effects
    Mix_Quit();

    //* GUI Wallpaper
    IMG_Quit();

}

//* Process Inputs
void process_inputs() {
    
    SDL_Event event;
    if(SDL_TryLockMutex(event_lock)){
        puts("process_inputs(): SDL_TryLockMutex failed!");
        return;
    };

    while (SDL_PollEvent(&event)) {

        switch(event.type)
        {
            case SDL_QUIT:
                GUI::Shutdown();
                break;
            case SDL_CONTROLLERDEVICEADDED:
                add_controller(event.cdevice.which);
                ShowTextOverlay("Gamepad Connected!");
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                remove_controller(event.cdevice.which);
                ShowTextOverlay("Gamepad Removed!");
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
                        GUI::Shutdown();
                        break; 
                    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                        //* Load GUI
                        if (bRunTests){
                            puts("User returned to tests.");
                            //PlaySound_Pipe();
                            GUI::PlaySound_UI(UI_SMB_PIPE);
                            TogglePauseEmulation();
                        }else if (is_rom_loaded()){
                            puts("User returned to game.");
                            //PlaySound_Pipe();
                            GUI::PlaySound_UI(UI_SMB_PIPE);
                            TogglePauseEmulation();
                        }
                        break;    
                }
                break;
        }
        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    SDL_UnlockMutex(event_lock);

}

//* Render ImGUI File Dialog
void render(){

    if(SDL_TryLockMutex(frame_lock)){
        puts("GUI::render(): SDL_TryLockMutex failed!");
        return;
    };

    SDL_RenderClear(GUIrenderer);
    if(SDL_RenderCopy(GUIrenderer, background, NULL, NULL)) {
        printf("failed to copy GUI background to render target: %s", SDL_GetError());
    }

    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    
    ImGui::Begin("NESalizer",NULL,ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("NESalizer for the Steam Link - Ported by TheCosmicSlug.");      

    //* Display current ROM info.

    if (is_rom_loaded()){
        ImGui::Text("Current ROM: '%s'.", basename(rom_filename()));
    }else{
        ImGui::Text("Current ROM: <none loaded>.");
    }

    //* GUI File Dialog
    static ImGuiFs::Dialog dlg;
    bool browseButtonPressed = false;
    if (ImGui::Button("Open ROM..")){
        browseButtonPressed = true;
    }

    const bool SaveStateButtonPressed = ImGui::Button("Save State"); 
    const bool LoadStateButtonPressed = ImGui::Button("Load State"); 
    const bool SettingsButtonPressed = ImGui::Button("Options"); 
    const bool AboutButtonPressed = ImGui::Button("About"); 
    const bool QuitButtonPressed = ImGui::Button("Exit NESalizer!"); 

    const char* chosenPath;
    chosenPath = dlg.chooseFileDialog(browseButtonPressed,"./roms/",".nes", "Choose a ROM.");

    ImGui::End();
    ImGui::Render();

    //* Load a new ROM?
    if (strlen(chosenPath)>0) {

        if (bRunTests){
            puts("NES ROM Tests disabled!");
            bRunTests = false;
            end_testing = true;
            StopEmulation();
            unload_rom();
        }
        else if(is_rom_loaded()){
            //* Unload any existing ROM
            StopEmulation();
            unload_rom();
        }
        if(LoadROM(dlg.getChosenPath())){
            //PlaySound_Coin();
            GUI::PlaySound_UI(UI_SMB_COIN);
            running_state = true;
            bShowGUI = false;
        };
    }

    //* Quit?
    if (QuitButtonPressed){
        Shutdown();
    }

    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(GUIrenderer);

    SDL_UnlockMutex(frame_lock);

}

void Shutdown(){
    
    //* Shut Everything Down
    if (bRunTests){
        end_testing = true;
    }
    if(is_rom_loaded()){
        unload_rom();
        running_state = true;
    }

    StopEmulation();
    bUserQuits = true;
    exitFlag = true;
}

}