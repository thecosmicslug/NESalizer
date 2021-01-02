#pragma once

#include "imgui.h"
#include "imgui_sdl.h"
#include "imguifilesystem.h"   

#include <string>

extern bool bShowGUI;
extern bool bShowOverlayText;

extern unsigned int OverlayTickCount;
extern std::string TextOverlayMSG;
extern char *statename;
extern int statenum;

namespace GUI {

void init(SDL_Window * scr, SDL_Renderer * rend);
void render();

void main_run();
void stop_main_run();
void unload_rom();

bool saveScreenshot(const std::string &file);
void SetROMStateFilename();
void ShowTextOverlay(std::string MSG);

}