//* ImGUI Config
#define IMGUI_USER_CONFIG "nesalizer_imgui_config.h"

#include "imgui/imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "imguifilesystem.h" 

extern unsigned int OverlayTickCount;
extern std::string TextOverlayMSG;

extern bool bShowGUI;
extern bool bShowOverlayText;

enum UISound
{
    UI_SMB_BUMP = 0,
    UI_SMB_COIN = 1,
    UI_SMB_PIPE = 2,
};

namespace GUI {

void init(SDL_Window* scr, SDL_Renderer* rend);
void deinit();
void process_inputs();
void render();

bool LoadROM(const char *filename);
void StopEmulation();
void PauseEmulation();
void ResumeEmulation();
void ShowTextOverlay(std::string MSG);
void IncreaseStateSlot();
void DecreaseStateSlot();
void LoadState();
void SaveState();
void Shutdown();

bool PlaySound_UI(UISound effect);
bool saveScreenshot(const std::string &file);

}