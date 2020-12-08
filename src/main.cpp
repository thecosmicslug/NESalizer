#include "common.h"

#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#include <SDL2/SDL.h>

char const *program_name;

static int emulation_thread(void*) {
    run();
    return 0;
}

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

    init_apu();
    init_mappers();
    
    // just check for a ROM filename right now
    program_name = argv[0] ? argv[0] : "nesalizer";
    if (argc != 2) {
        // TODO: Add GUI File Dialog to select ROM
        load_rom("Mario(E).nes", true);   
    }
    else 
    {
         load_rom(argv[1],  true);      
    }

    init_sdl();
    SDL_ShowCursor(SDL_DISABLE);
    SDL_Thread *emu_thread;
    
    // Create a separate emulation thread and use this thread as the rendering thread
    fail_if(!(emu_thread = SDL_CreateThread(emulation_thread, "emulation", 0)), "failed to create emulation thread: %s", SDL_GetError());
    
    sdl_thread();
    SDL_WaitThread(emu_thread, 0);
    deinit_sdl();

    puts("---- NESalizer-SteamLink Shut down cleanly!");
    puts("-------------------------------------------------------");
}
