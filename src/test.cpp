#include "common.h"
#include "cpu.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#include "sdl_frontend.h"

#include <fstream>
#include <stdio.h>

#define BUF_SIZE 65536

int TotalTests;
int CurrentTestNum;

bool end_testing;
char const *current_filename;
char const *testlist_filename;

unsigned int StartAllTestTime = 0, currentTestTime;

void report_status_and_end_test(uint8_t status, char const *msg) {

    unsigned int currentTime;
    unsigned int TimeTaken;

    currentTime = SDL_GetTicks(); 
    TimeTaken = currentTime - currentTestTime;

    if (status == 0){
        printf("%-60s OK\n", current_filename);
        printf("Time Taken: %dms\n", TimeTaken);

    }else{
        printf("%-60s FAILED\nvvv TEST OUTPUT START vvv\n%s\n^^^ TEST OUTPUT END ^^^\n", current_filename, msg);
        printf("Time Taken: %dms\n", TimeTaken);
    }

    end_emulation();
}

static void run_test(char const *file) {

    //* Time the test
    currentTestTime = SDL_GetTicks();
    current_filename = file;

    //* Show Overlay UI

    std::string tmpstr =  '[' + std::__cxx11::to_string(CurrentTestNum) + '/' + std::__cxx11::to_string(TotalTests) + ']';
    tmpstr += " '";
    tmpstr += file;
    tmpstr += "'";
    GUI::ShowTextOverlay(tmpstr);

    //* Run Test
    load_rom(file);
    run();
    if (end_testing){
        return;
    }
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

    printf("Running NES tests from: %s\n", testlist_filename);

    unsigned int currentTime, TimeTaken;
    StartAllTestTime = SDL_GetTicks();

    //* Read the ROM list one line at a time.
    std::ifstream file(testlist_filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            //* Run the Test ROM
            CurrentTestNum = CurrentTestNum + 1;
            if (end_testing) goto end;
            RUN_TEST(line.c_str());
        }
        file.close();
    }

    //* Log Output
    currentTime = SDL_GetTicks();
    TimeTaken = currentTime - StartAllTestTime;
    GUI::ShowTextOverlay("NES Tests Complete!");
    printf("NES Tests Complete in %d secs.\n", TimeTaken / 1000);

    //* End of Testing
    GUI::StopEmulation();
    bRunTests = false;
    bShowGUI = true;
    return;

    #undef RUN_TEST

end:
    //* Premature end of testing.
    puts("run_tests() finished early.");
}

int CountTestList(char const *testlist){

    FILE *fileptr;
    char buf[BUF_SIZE];
    int counter = 0;

    fileptr = fopen(testlist, "r");
    if (fileptr == NULL){
        printf("Could not open file.");
        return 0;
    }

    for(;;)
    {
        size_t res = fread(buf, 1, BUF_SIZE, fileptr);
        if (ferror(fileptr)){
            return 0;
        }
            
        int i;
        for (i = 0; i < res; i++){
            if (buf[i] == '\n'){
                counter++;
            }
        }

        if (feof(fileptr)){
            break;
        }
            
    }

    fclose(fileptr);
    return counter;


}

void setup_tests(char const *testlist){

    CurrentTestNum = 0;
    TotalTests = CountTestList(testlist);
    testlist_filename = testlist;
    
    end_testing = false;
    bRunTests=true;
    bShowGUI=false;

}