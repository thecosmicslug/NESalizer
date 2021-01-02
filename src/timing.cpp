#include "common.h"

#include "mapper.h"
#include "rom.h"
#include "timing.h"

double cpu_clock_rate;
double ppu_clock_rate;
double ppu_fps;

void init_timing_for_rom() {
    if (is_pal) {
        double master_clock_rate = 26601712.0;
        cpu_clock_rate           = master_clock_rate/16.0; //* ~1.66 MHz
        ppu_clock_rate           = master_clock_rate/5.0; //* ~5.32 MHz
        ppu_fps                  = ppu_clock_rate/(341*312); //* ~50.0 FPS
    }
    else {
        double master_clock_rate = 21477272.0;
        cpu_clock_rate           = master_clock_rate/12.0; //* ~1.79 MHz
        ppu_clock_rate           = master_clock_rate/4.0; //* ~5.37 MHz
        ppu_fps                  = ppu_clock_rate/(341*261 + 340.5); //* ~60.1 FPS
    }
}