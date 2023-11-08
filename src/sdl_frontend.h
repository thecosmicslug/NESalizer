
#include "imgui/imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "imguifilesystem.h"   
#include <string>

extern bool bShowGUI;
extern bool bShowOverlayText;
extern unsigned int OverlayTickCount;
extern std::string TextOverlayMSG;

extern bool bRunTests;
extern char *testfilename;

extern bool bForcePAL;
extern bool bForceNTSC;

extern char *statename;
extern int statenum;
extern char *savename;


namespace GUI {

void init(SDL_Window* scr, SDL_Renderer* rend);
void render();
void main_run();
void stop_main_run();
void unload_rom();

bool saveScreenshot(const std::string &file);
void SetROMStateFilename();
void SetSRAMFilename();
void ShowTextOverlay(std::string MSG);
void PlaySound_Coin();
void PlaySound_Bump();
void PlaySound_Pipe();
}