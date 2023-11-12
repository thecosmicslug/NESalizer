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

namespace GUI {

void init(SDL_Window* scr, SDL_Renderer* rend);
void deinit();
void process_inputs();
void render();

bool LoadROM(const char *filename);
void StopEmulation();
void TogglePauseEmulation();
void ShowTextOverlay(std::string MSG);
void IncreaseStateSlot();
void DecreaseStateSlot();
void LoadState();
void SaveState();

void PlaySound_Coin();
void PlaySound_Bump();
void PlaySound_Pipe();

bool saveScreenshot(const std::string &file);

}