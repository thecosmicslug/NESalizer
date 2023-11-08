#include <SDL2/SDL.h>
#include <unistd.h>

#include "common.h"
#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#include "sdl_frontend.h"

#define IMGUI_USER_CONFIG "nesalizer_imgui_config.h"

char const *program_name;

int main(int argc, char *argv[]) {

    setvbuf (stdout, NULL, _IONBF, 0);
    printf("-------------------------------------------------------\n");
    printf("---------- NESalizer-SteamLink ------------------------\n");
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
    
    //* Parsing command-line arguments first.
    int opt;
    while ((opt = getopt(argc, argv, "t:pnf:")) != -1) {
        switch (opt) {
            case 't':
                //* Run NES Tests
                if (optarg != NULL){
                    testfilename = optarg;
                    bRunTests=true;
                    bShowGUI=false;
                }
                break;
            case 'p':
                //* Force PAL-Mode
                if (!bForceNTSC){
                    bForcePAL=true;
                }else{
                    printf("Cannot force both PAL and NTSC.. Ignoring both options\n");
                    bForcePAL=false;
                    bForceNTSC=false;
                }
                break;
            case 'n':
                //* Force NTSC-Mode
                if (!bForcePAL){
                    bForceNTSC=true;
                }else{
                    printf("Cannot force both PAL and NTSC.. Ignoring both options\n");
                    bForcePAL=false;
                    bForceNTSC=false;
                }
                break;
            case 'f':
                if (!bRunTests){
                    //* Try Loading the supplied ROM
                    if(load_rom(optarg)){
                        GUI::SetROMStateFilename();
                        std::string tmpstr = "ROM '";
                        tmpstr  += basename(fname);
                        tmpstr  += "' Loaded!";
                        GUI::ShowTextOverlay(tmpstr);
                        bShowGUI=false;
                    }else{
                        bShowGUI=true;
                    };
                }
                break;
            //TODO: Add Verbose & Extra verbose options.
        default:
            bShowGUI=true;
        }
    }

    //* Our Main Execution Loop
    while (true){

        if (bShowGUI){
            //* Our ImGUI File Dialog
            //SDL_Delay(100);
            process_gui_events();
            GUI::render();
        }else{
            //* Run Emulation!
            puts("main() bShowGUI = 'false'");
            GUI::main_run();
        }
    }
    //* We never reach here.
    deinit_sdl();
    printf("---- NESalizer-SteamLink Shut down cleanly! ----------");
    printf("------------------------------------------------------");
}
