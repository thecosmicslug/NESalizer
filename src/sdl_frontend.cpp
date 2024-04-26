
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

SDL_Texture *game_background;
SDL_Texture *nes_background;
SDL_Renderer *GUIrenderer;

using std::string;

void SetIMGUI_Style(){

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
    style.ScrollbarRounding = 0;

    style.Colors[ImGuiCol_Text]                  = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.00f, 0.00f, 0.01f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.90f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.90f, 0.65f, 0.65f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.40f, 0.40f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.00f, 0.00f, 0.87f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.01f, 0.01f, 0.02f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.55f, 0.53f, 0.55f, 0.51f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.56f, 0.56f, 0.56f, 0.91f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.90f, 0.90f, 0.90f, 0.83f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.70f, 0.70f, 0.70f, 0.62f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.30f, 0.30f, 0.30f, 0.84f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.48f, 0.72f, 0.89f, 0.49f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.50f, 0.69f, 0.99f, 0.68f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.30f, 0.69f, 1.00f, 0.53f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.44f, 0.61f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.38f, 0.62f, 0.83f, 1.00f);
    style.Colors[ImGuiCol_Separator]             = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.70f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.90f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
    style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

}

namespace GUI
{

bool PlaySound_UI(UISound effect){
    if(!Mix_PlayChannel(-1, _sample[effect], 0)){
        //printf("Unable to play WAV file: %s\n", Mix_GetError());
        return false;
    };
    return true;
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

bool SaveState(){

    //* Save to current state-slot
    if (save_state(statename)){

        std::string tmpstr = "State  '";
        tmpstr += basename(statename);
        tmpstr  += "'  Saved!";
        ShowTextOverlay(tmpstr);
        GUI::PlaySound_UI(UI_SMB_COIN);
        return true;

    }else{

        std::string tmpstr = "Failed to save-state '";
        tmpstr += basename(statename);
        tmpstr += "'";
        ShowTextOverlay(tmpstr);
        GUI::PlaySound_UI(UI_SMB_BUMP);
        return false;
    };

}

bool LoadState(){

    //* Load Savestate from file
    if(load_state(statename)){

        std::string tmpstr = "State  '";
        tmpstr += basename(statename);
        tmpstr += "'  Loaded!";
        GUI::PlaySound_UI(UI_SMB_COIN);
        ShowTextOverlay(tmpstr);
        return true;

    }else{

        std::string tmpstr = "'";
        tmpstr += basename(statename);
        tmpstr += "'  Not Found!";
        ShowTextOverlay(tmpstr);
        GUI::PlaySound_UI(UI_SMB_BUMP);
        return false;
    };
}

void IncreaseStateSlot(){
    
    if (is_rom_loaded()){
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
        GUI::PlaySound_UI(UI_SMB_COIN);
    }

}

void DecreaseStateSlot(){

    if (is_rom_loaded()){
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
        GUI::PlaySound_UI(UI_SMB_COIN);
    }

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

bool GetEmulationBackground() {

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

    game_background = SDL_CreateTextureFromSurface(GUIrenderer, _surface);
    if (!game_background) {
        printf("GetEmulationBackground(): SDL_CreateTextureFromSurface failed! %s\n", SDL_GetError());
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
void PauseEmulation(){

    //* Get background for GUI.
    GetEmulationBackground();

    if (bVerbose){
        puts("running_state = 'false'");
    }

    //NOTE: need to test these delays. 200 worked.
    SDL_Delay(50);

    bShowGUI = true;
    running_state = false;

}

//* Play/stop the emulation. 
void ResumeEmulation(){

    //* Clear old background.
    if (game_background){
        SDL_DestroyTexture(game_background);
        game_background = nullptr;
    }

    if (bVerbose){
        puts("running_state = 'true'");
    }

    //NOTE: need to test these delays. 200 worked.
    SDL_Delay(200);

    bShowGUI = false;
    running_state = true;

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

    //* Load our default background.
    SDL_Surface *backSurface = IMG_Load("res/wallpaper.png");
    if(!backSurface) {
        printf("IMG_Load: %s\n", IMG_GetError());
    }
    else
    {
        nes_background = SDL_CreateTextureFromSurface(GUIrenderer, backSurface);
        SDL_FreeSurface(backSurface);
        if (!nes_background){
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

    SetIMGUI_Style();
    //ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(scr, GUIrenderer);
    ImGui_ImplSDLRenderer_Init(GUIrenderer);
}

//* Unload GUI bits
void deinit(){

    //* GUI Overlay
    if (!overlay_tex){
        SDL_DestroyTexture(overlay_tex);
    }

    if(!game_background){
        SDL_DestroyTexture(game_background);
    }

    if(!nes_background){
        SDL_DestroyTexture(nes_background);
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
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        //* Change Saveslot -1 
                        GUI::DecreaseStateSlot();
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        //* Change Saveslot +1 
                        GUI::IncreaseStateSlot();
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                        //* Return to Emulation
                        if (bRunTests){
                            puts("User returned to tests.");
                            GUI::PlaySound_UI(UI_SMB_PIPE);
                            ResumeEmulation();
                        }else if (is_rom_loaded()){
                            puts("User returned to game.");
                            GUI::PlaySound_UI(UI_SMB_PIPE);
                            ResumeEmulation();
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

    //SDL_RenderClear(GUIrenderer);
    if (!game_background){
        if(SDL_RenderCopy(GUIrenderer, nes_background, NULL, NULL)) {
            printf("failed to copy GUI background to render target: %s", SDL_GetError());
        }
    }else{
        if(SDL_RenderCopy(GUIrenderer, game_background, NULL, NULL)) {
            printf("failed to copy GUI background to render target: %s", SDL_GetError());
        }
    }

    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    
    ImGui::Begin("NESalizer",NULL,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("NESalizer for the Steam Link - Ported by TheCosmicSlug.");      
    ImGui::Separator();

    bool SaveStateButtonPressed;
    bool LoadStateButtonPressed;

    //* GUI File Dialog
    static ImGuiFs::Dialog rom_dlg;
    static ImGuiFs::Dialog test_dlg;

    const bool RomBrowseButtonPressed = ImGui::Button("Open ROM..");
    ImGui::SameLine();

    //* Display current ROM info.
    if (is_rom_loaded()){
        if (rom_filename() != NULL){
            ImGui::Text("Current ROM: '%s'.", basename(rom_filename()));
            SaveStateButtonPressed = ImGui::Button("Save State");
            ImGui::SameLine();
            LoadStateButtonPressed = ImGui::Button("Load State"); 
        }else{
            ImGui::Text("Current ROM: <none loaded>.");
            ImGui::BeginDisabled();
            SaveStateButtonPressed = ImGui::Button("Save State");
            ImGui::SameLine();
            LoadStateButtonPressed = ImGui::Button("Load State"); 
            ImGui::EndDisabled();
        }
    }else{
        ImGui::Text("Current ROM: <none loaded>.");
        ImGui::BeginDisabled();
        SaveStateButtonPressed = ImGui::Button("Save State");
        ImGui::SameLine();
        LoadStateButtonPressed = ImGui::Button("Load State"); 
        ImGui::EndDisabled();
    }

    if(bRunTests){
        ImGui::BeginDisabled();
    }

    const bool TestButtonPressed = ImGui::Button("NES Tests..");

    if(bRunTests){
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Text("[ Running test %i out of %i. ]", CurrentTestNum, TotalTests);
    }else{
        ImGui::SameLine();
        ImGui::Text("[ NES Tests not running. ]");
    }

    ImGui::Separator();

    const bool QuitButtonPressed = ImGui::Button("Exit NESalizer!"); 

    const char* RomChosenPath = "";
    const char* TestChosenPath = "";
    RomChosenPath = rom_dlg.chooseFileDialog(RomBrowseButtonPressed,"./roms/",".nes", "Choose a ROM.");
    TestChosenPath = test_dlg.chooseFileDialog(TestButtonPressed,"./",".txt", "Choose Test List.");

    //* Load a new ROM
    if (strlen(RomChosenPath)>0) {
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
        if(LoadROM(RomChosenPath)){
            //* Return to Emulation
            //RomBrowseButtonPressed = false;
            GUI::PlaySound_UI(UI_SMB_COIN);
            ResumeEmulation();
        }
    }

    //* Load a test list
    if (strlen(TestChosenPath)>0){
        
        if(is_rom_loaded()){
            //* Unload any existing ROM
            StopEmulation();
            unload_rom();
        }
        setup_tests(TestChosenPath);
        //TestButtonPressed = false;
        GUI::PlaySound_UI(UI_SMB_COIN);
        ResumeEmulation();
    }

    ImGui::End();
    ImGui::Render();

    //* Save State
    if (SaveStateButtonPressed){
        if (SaveState()){
            //ResumeEmulation();
        }
    }

    //* Load State
    if (LoadStateButtonPressed){
        if (LoadState()){
            ResumeEmulation();
        }
    }

    //* Quit?
    if (QuitButtonPressed){
        Shutdown();
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
            SDL_RenderCopy(GUIrenderer, overlay_tex, NULL, &dstrect);
        }else{
            //* Disable the overlay now 
            SDL_DestroyTexture(overlay_tex);
            bShowOverlayText=false;
        }
    } 


    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(GUIrenderer);

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