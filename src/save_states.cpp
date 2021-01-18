#include "common.h"

#include "apu.h"
#include "audio.h"
#include "controller.h"
#include "cpu.h"
#include "input.h"
#include "ppu.h"
#include "mapper.h"
#include "rom.h"
#include "save_states.h"
#include "timing.h"
#include "sdl_frontend.h"

//* Buffer for the save state.
static uint8_t *state;
static size_t state_size;

template<bool calculating_size, bool is_save>
static size_t transfer_system_state(uint8_t *buf) 
{
    uint8_t *tmp = buf;

    transfer_apu_state<calculating_size, is_save>(buf);
    transfer_cpu_state<calculating_size, is_save>(buf);
    transfer_ppu_state<calculating_size, is_save>(buf);
    transfer_controller_state<calculating_size, is_save>(buf);
    transfer_input_state<calculating_size, is_save>(buf);

    if (calculating_size)
        mapper_fns.state_size(buf);
    else {
        if (is_save)
            mapper_fns.save_state(buf);
        else
            mapper_fns.load_state(buf);
    }

    //* Return size of state in bytes
    return buf - tmp;
}

void save_state() 
{
    //* create a savestate
    transfer_system_state<false, true>(state);

    FILE * pFile;
    pFile = fopen (statename, "wb");
    if (pFile != NULL)
    {
        //* Write it to disk
        printf("saving to '%s'\n", statename);
        fwrite (state , sizeof(uint8_t), state_size, pFile);
        fclose (pFile);
        std::string tmpstr = "State  '";
        tmpstr += basename(statename);
        tmpstr  += "'  Saved!";
        GUI::ShowTextOverlay(tmpstr);
        GUI::PlaySound_Coin();
    }else
    {
        GUI::PlaySound_Bump();
        printf("failed to open '%s'\n", statename);
    }
}

void load_state() 
{
    FILE * pFile;
    pFile = fopen (statename, "r");
    if (pFile)
    {
        fclose (pFile);
        printf("loading savestate '%s'\n", basename(statename));
        state = get_file_buffer(statename, state_size);
        transfer_system_state<false, false>(state);
        std::string tmpstr = "State  '";
        tmpstr += basename(statename);
        tmpstr += "'  Loaded!";
        GUI::PlaySound_Coin();
        GUI::ShowTextOverlay(tmpstr);
    }else
    {
        std::string tmpstr = "'";
        tmpstr += basename(statename);
        tmpstr += "'  Not Found!";
        GUI::ShowTextOverlay(tmpstr);
        GUI::PlaySound_Bump();
        printf("failed to load savestate '%s'\n", basename(statename));
    }

}

void init_save_states_for_rom() 
{   
    state_size = transfer_system_state<true, false>(0);
    if(!bRunTests){
        printf("save state size: %zu bytes\n", state_size);
    }
    if(!(state = new (std::nothrow) uint8_t[state_size])) {
        printf("failed to allocate %zu-byte buffer for save state", state_size);
    }
}

void deinit_save_states_for_rom() 
{
    free_array_set_null(state);
}