#include <SDL2/SDL.h>

#include "common.h"
#include "cpu.h"
#include "apu.h"
#include "mapper.h"
#include "test.h"

#include "sdl_backend.h"
#include "sdl_frontend.h"

//* Program Entry Point
int main(int argc, char *argv[]) {

    setvbuf (stdout, NULL, _IONBF, 0);
    printf("NESalizer-SteamLink Starting...\n");

    //* Load NES Modules.
    init_apu();
    init_mappers();

    //* Parsing command-line arguments.
    int opt;
    while ((opt = getopt(argc, argv, "t:pnf:vd")) != -1) {
        switch (opt) {
            case 'v':
                puts("Verbose Mode Enabled.");
                bVerbose = true;
                break;
            case 'd':
                //* Debug Mode will output even more, lagging emulation.
                puts("Debug Mode Enabled, even more verbose!");
                bVerbose = true;
                bExtraVerbose = true;
                break;
            case 't':
                //* Run NES Tests
                if (optarg != NULL){
                    setup_tests(optarg);
                }
                break;
            case 'p':
                //* Force PAL-Mode
                if (!bForceNTSC){
                    bForcePAL=true;
                }else{
                    puts("Cannot force both PAL and NTSC.. Ignoring both options");
                    bForcePAL=false;
                    bForceNTSC=false;
                }
                break;
            case 'n':
                //* Force NTSC-Mode
                if (!bForcePAL){
                    bForceNTSC=true;
                }else{
                    puts("Cannot force both PAL and NTSC.. Ignoring both options");
                    bForcePAL=false;
                    bForceNTSC=false;
                }
                break;
            case 'f':
                if (!bRunTests){
                    //* Try Loading the supplied ROM
                    if(GUI::LoadROM(optarg)){
                        bShowGUI=false;
                    }else{
                        bShowGUI=true;
                    };
                }
                break;
        default:
            bShowGUI=true;
        }
    }

    //* Show SDL version info
    if (bVerbose){
        SDL_version sdl_compiled_version, sdl_linked_version;
        SDL_VERSION(&sdl_compiled_version);
        SDL_GetVersion(&sdl_linked_version);

        printf("Compiled against SDL %d.%d.%d, linked to SDL %d.%d.%d.\n",
            sdl_compiled_version.major, sdl_compiled_version.minor, sdl_compiled_version.patch,
            sdl_linked_version.major, sdl_linked_version.minor, sdl_linked_version.patch);
    }

    //* Setup SDL Backend.
    init_sdl();

    if (bShowGUI){
        puts("Showing GUI to user.");    
    }

    //* Our Main Execution Loop
    while (!bUserQuits){

        if (bShowGUI){
            //* Our GUI
            GUI::process_inputs();
            GUI::render();
        }else{
            //* Run Emulation!
            RunEmulation();
        }
    }
    //* End, Clean up.
    deinit_sdl();

    //* Last statement!
    puts("NESalizer shutdown cleanly!");
    
}
