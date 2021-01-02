extern double cpu_clock_rate;
extern double ppu_clock_rate;
extern double ppu_fps;

void init_timing_for_rom();

//* Hack to get a C++03 compile-time constant
unsigned const pal_milliframes_per_second = 50007;
