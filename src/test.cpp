#include "common.h"
#include "cpu.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#include "sdl_frontend.h"

#include <fstream>

bool end_testing;
static char const *filename;

unsigned int StartAllTestTime = 0, currentTestTime;

void report_status_and_end_test(uint8_t status, char const *msg) {

    unsigned int currentTime;
    unsigned int TimeTaken;

    currentTime = SDL_GetTicks(); 
    TimeTaken = currentTime - currentTestTime;

    if (status == 0){
        printf("%-60s OK\n", filename);
        printf("Time Taken: %dms\n", TimeTaken);

    }else{
        printf("%-60s FAILED\nvvv TEST OUTPUT START vvv\n%s\n^^^ TEST OUTPUT END ^^^\n", filename, msg);
        printf("Time Taken: %dms\n", TimeTaken);
    }

    end_emulation();
}

static void run_test(char const *file) {
    currentTestTime = SDL_GetTicks();
    filename = file;
    GUI::ShowTextOverlay(file);
    load_rom(file);
    run();
    unload_rom();
}

void run_tests() {

    //* These can't be automated as easily:
    //   cpu_dummy_reads
    //   sprite_hit_tests_2005.10.05
    //   sprite_overflow_tests
    //*
    //* Tests that require manual inspection:
    //   dpcm_letterbox
    //   nmi_sync
    //*
    //* Look into these too:
    //   dmc_tests

    //* Do it like this to avoid extra newlines being printed when aborting testing
    #define RUN_TEST(file) run_test(file); if (end_testing) goto end;

    printf("Running NES tests from: %s\n", testfilename);

    unsigned int currentTime, TimeTaken;
    StartAllTestTime = SDL_GetTicks();

    //* Read the ROM list one line at a time.
    std::ifstream file(testfilename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            //* Run the Test ROM
            RUN_TEST(line.c_str());
        }
        file.close();
    }

    //* Log Timing
    currentTime = SDL_GetTicks();
    TimeTaken = currentTime - StartAllTestTime;

    GUI::ShowTextOverlay("NES Tests Complete!");
    printf("NES Tests Complete!\n");
    printf("TOTAL Time Taken: %d secs\n", TimeTaken / 1000);

    //* End of the Tests
    end_emulation();
    exit_sdl_thread();
    GUI::stop_main_run();
    bUserQuits = true;
    return;

    #undef RUN_TEST

end:
    //* premature quit
    end_emulation();
    exit_sdl_thread();
    GUI::stop_main_run();
    if (bRunTests){
        bUserQuits = true;
    }
}
