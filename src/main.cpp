#include <SDL2/SDL.h>

#include "common.h"
#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#include "sdl_frontend.h"

char const *program_name;


int main(int argc, char *argv[]) {

    printf("-------------------------------------------------------\n");
    printf("---------- NESalizer-SteamLink Booted up!    ----------\n");
    printf("-------------------------------------------------------\n");

    SDL_version sdl_compiled_version, sdl_linked_version;
    SDL_VERSION(&sdl_compiled_version);
    SDL_GetVersion(&sdl_linked_version);

    printf("Using SDL backend. Compiled against SDL %d.%d.%d, linked to SDL %d.%d.%d.\n",
        sdl_compiled_version.major, sdl_compiled_version.minor, sdl_compiled_version.patch,
           sdl_linked_version.major, sdl_linked_version.minor, sdl_linked_version.patch);

    //* Setup Audio/Video Outputs with SDL & ImGUI
    init_sdl();
    SDL_ShowCursor(SDL_DISABLE);
    
    init_apu();
    init_mappers();
    
    //* Check for a ROM Filename as supplied argument 
    program_name = argv[0] ? argv[0] : "nesalizer";
    if (argc != 2) {
        //* No ROM supplied, Select from file dialog
        bShowGUI=true;
    }else{
        if(load_rom(argv[1])){
            GUI::SetROMStateFilename();
            std::string tmpstr = "ROM  '";
            tmpstr  += basename(fname);
            tmpstr  += "'  Loaded!";
            GUI::ShowTextOverlay(tmpstr);
        }else{
            bShowGUI=true;
        };
           
    }
   
    while (true)
    {
        if (bShowGUI)
        {
            //* Our ImGUI File Dialog
            process_events();
            GUI::render();
        
        }else{
            //* Run Emulation!
            GUI::main_run();
        }
    }

    deinit_sdl();
    printf("---- NESalizer-SteamLink Shut down cleanly!");
    printf("------------------------------------------------------");
}
