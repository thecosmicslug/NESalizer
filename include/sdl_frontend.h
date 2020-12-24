#pragma once

// GUI Dialog WIP
#include "imgui.h"
#include "imgui_sdl.h"
#include "imguifilesystem.h"   

extern bool bShowGUI;

namespace GUI {

void init(SDL_Window * scr, SDL_Renderer * rend);
void render();

void main_run();
void stop_main_run();
void unload_rom();

void process_gui_events();

}