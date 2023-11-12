#include "common.h"

#include "apu.h"
#include "audio.h"
#include "mapper.h"
#include "md5.h"
#include "ppu.h"
#include "rom.h"
#include "cpu.h"

#include "save_states.h"
#include "timing.h"
#include "sdl_backend.h"

uint8_t *prg_base;
unsigned prg_16k_banks;
uint8_t *chr_base;
unsigned chr_8k_banks;
uint8_t *wram_base;
unsigned wram_8k_banks;

bool is_pal;
bool has_battery;
bool has_trainer;
bool is_vs_unisystem;
bool is_playchoice_10;
bool has_bus_conflicts;
bool chr_is_ram;
bool rom_loaded;

Mapper_fns mapper_fns;

uint8_t *rom_buf;
const char *fname;

//* SRAM savefile
char *savename;

char const *const mirroring_to_str[N_MIRRORING_MODES] =
  { "horizontal",
    "vertical",
    "one-screen, low",
    "one-screen, high",
    "four-screen" };

static void do_rom_specific_overrides();

void SetSRAMFilename(char const *romfile){
    //* Strip Path , Add State folder , Replace extension
    string filename = basename(romfile);
    string path = "saves/" + filename;
    replaceExt(path, "sav");

    //* Convert back to char-array
    savename = new char [path.length()+1];
    strcpy (savename, path.c_str());

}

char* const GetSRAMFilename(){
    //* Strip Path , Add State folder , Replace extension
    return savename;
}

bool load_rom(const char *filename) {

    if (!bRunTests){
        printf("Loading ROM '%s'\n", basename(filename));
    }

    size_t rom_buf_size;
    rom_buf = get_file_buffer(filename, rom_buf_size);
    is_pal = strstr(filename, "(E)") || strstr(filename, "PAL");

    if (!bRunTests){
        printf("guessing %s based on filename alone\n", is_pal ? "PAL" : "NTSC");
    }

    if(rom_buf_size < 16) {
        printf("'%s' is too short to be a valid iNES file (is %zu bytes - not even enough to hold the 16-byte "
        "header)", filename, rom_buf_size);
        return false;
    }

    if(!MEM_EQ(rom_buf, "NES\x1A")) {
        printf("'%s' does not start with the expected byte sequence 'N', 'E', 'S', 0x1A", filename);
        return false;
    }

    prg_16k_banks = rom_buf[4];
    chr_8k_banks  = rom_buf[5];

    if (!bRunTests){
        printf("PRG ROM size: %u KB\nCHR ROM size: %u KB\n", 16*prg_16k_banks, 8*chr_8k_banks);
    }

    if(prg_16k_banks == 0) { //NOTE: This makes sense for NES 2.0
        printf("the iNES header specifies zero banks of PRG ROM (program storage), which makes no sense");
        return false;
    }
    if(!is_pow_2_or_0(prg_16k_banks) || !is_pow_2_or_0(chr_8k_banks)) {
        printf("non-power-of-two PRG and CHR sizes are not supported yet");
        return false;
    }

    size_t const min_size = 16 + 512*has_trainer + 0x4000*prg_16k_banks + 0x2000*chr_8k_banks;
    if(rom_buf_size < min_size) {
        printf("'%s' is too short to hold the specified amount of PRG (program data) and CHR (graphics data) "
            "ROM - is %zu bytes, expected at least %zu bytes (16 (header) + %s%u*16384 (PRG) + %u*8192 (CHR))",
            filename, rom_buf_size, min_size, has_trainer ? "512 (trainer) + " : "", prg_16k_banks, chr_8k_banks);
        return false;
    }

    //* Possibly updated with the high nibble below
    unsigned mapper;
    mapper = rom_buf[6] >> 4;
    bool const is_nes_2_0 = (rom_buf[7] & 0x0C) == 0x08;

    if (bVerbose){
        if (!bRunTests){
            printf(is_nes_2_0 ? "in NES 2.0 format\n" : "in iNES format\n");
        }
    }

    //* Assume we're dealing with a corrupted header (e.g. one containing
    //* "DiskDude!" in bytes 7-15) if the ROM is not in NES 2.0 format and bytes
    //* 12-15 are not all zero
    if (!is_nes_2_0 && !MEM_EQ(rom_buf + 12, "\0\0\0\0"))
        printf("header looks corrupted (bytes 12-15 not all zero) - ignoring byte 7\n");
    else {
        is_vs_unisystem  = rom_buf[7] & 1;
        is_playchoice_10 = rom_buf[7] & 2;  
        mapper |= (rom_buf[7] & 0xF0);
    }

    if (bVerbose){
        if (!bRunTests){
            printf("mapper: %u\n", mapper);
        }
    }


    if (rom_buf[6] & 8)
        //* The cart contains 2 KB of additional CIRAM (nametable memory) and uses four-screen (linear) addressing
        mirroring = FOUR_SCREEN;
    else
        mirroring = rom_buf[6] & 1 ? VERTICAL : HORIZONTAL;

    if (bVerbose){
        if ((has_battery = rom_buf[6] & 2)) printf("has battery\n");
    }
    if (bVerbose){
        if ((has_trainer = rom_buf[6] & 4)) printf("has trainer\n");
    }

    //* Set pointers, allocate memory areas, and do misc. setup
    prg_base = rom_buf + 16 + 512*has_trainer;

    //* Default
    has_bus_conflicts = false;
    do_rom_specific_overrides();

    //* Here we apply our Force Region if needed.
    if (bForcePAL)
    {
        is_pal=true;
        printf("Forcing PAL Region\n");
    }
    else if(bForceNTSC)
    {
        is_pal=false;
        printf("Forcing NTSC Region\n");
    }

    //* Needs to come after a possible override
    prerender_line = is_pal ? 311 : 261;

    if (bVerbose){
        if (!bRunTests){
            printf("mirroring: %s\n", mirroring_to_str[mirroring]);
        }
    }

    if(!(ciram = alloc_array_init<uint8_t>(mirroring == FOUR_SCREEN ? 0x1000 : 0x800, 0xFF))) {
        printf("failed to allocate %u bytes of nametable memory", mirroring == FOUR_SCREEN ? 0x1000 : 0x800);
        return false;
    }

    if (mirroring == FOUR_SCREEN || mapper == 7){
        //* Assume no WRAM when four-screen, per
        //* http://*wiki.nesdev.com/w/index.php/INES_Mapper_004. Also assume no
        //* WRAM for AxROM (mapper 7) as having it breaks Battletoads & Double
        //* Dragon. No AxROM games use WRAM.
        wram_base = wram_6000_page = NULL;
    }else{
        //* iNES assumes all carts have 8 KB of WRAM. For MMC5, assume the cart
        //* has 64 KB.
        wram_8k_banks = (mapper == 5) ? 8 : 1;
        if(!(wram_6000_page = wram_base = alloc_array_init<uint8_t>(0x2000*wram_8k_banks, 0xFF))) {
            printf("failed to allocate %u KB of WRAM", 8*wram_8k_banks);
            return false;
        }
    }


    if ((chr_is_ram = (chr_8k_banks == 0))) {
        //* Assume cart has 8 KB of CHR RAM, except for Videomation which has 16 KB
        chr_8k_banks = (mapper == 13) ? 2 : 1;
        if(!(chr_base = alloc_array_init<uint8_t>(0x2000*chr_8k_banks, 0xFF))) {
            printf("failed to allocate %u KB of CHR RAM", 8*chr_8k_banks);
            return false;
        }
    }else{
        chr_base = prg_base + 16*1024*prg_16k_banks;
    } 


    if(is_nes_2_0) {
        if (!bRunTests){
            printf("NES 2.0 not yet supported");
        }
        return false;
    }


    if(!mapper_fns_table[mapper].init) {
        if (!bRunTests){
            printf("mapper %u not supported\n", mapper);
        }
        return false;
    }

    mapper_fns = mapper_fns_table[mapper];
    mapper_fns.init();

    init_timing_for_rom();
    init_apu_for_rom();
    init_audio_for_rom();
    init_ppu_for_rom();
    init_save_states_for_rom();

    fname = filename;

    //* Check if we should look for SRAM 
    if(has_battery) //* Only needed for ROMs with battery
    {
        SetSRAMFilename(filename);
        FILE * pFile;
        pFile = fopen (savename, "rb");
        if (pFile != NULL)
        {
            //* LOAD SRAM BECAUSE IT EXISTS
            size_t savesize = 8192;
            fclose (pFile);
            if (bVerbose){
                printf("Loading SRAM from '%s'\n", savename);
            }
            wram_6000_page = get_file_buffer(savename,savesize);
        }else{
            if (bVerbose){
                printf("No SRAM found!\n");
            }
        }
    }

    set_rom_loaded(true);
    return true;
}

void unload_rom() {

    //* Save SRAM?
    if(has_battery){
        write_SRAM();
    }
    //* Flush any pending audio samples
    end_audio_frame();

    //* Clear Buffers
    free_array_set_null(rom_buf);
    free_array_set_null(ciram);
    if (chr_is_ram){
        free_array_set_null(chr_base);
    }
    free_array_set_null(wram_base);

    deinit_audio_for_rom();
    deinit_save_states_for_rom();
    set_rom_loaded(false);
}

//* ROM detection from a PRG MD5 digest. Needed to infer and correct information
//* for some ROMs.

static void correct_mirroring(Mirroring m) {
    if (mirroring != m) {
        printf("Correcting mirroring from %s to %s based on ROM checksum\n",
               mirroring_to_str[mirroring], mirroring_to_str[m]);
        mirroring = m;
    }
}

static void enable_bus_conflicts() {
    puts("Enabling bus conflicts based on ROM checksum");
    has_bus_conflicts = true;
}

static void set_pal() {
    puts("Setting PAL mode based on ROM checksum");
    is_pal = true;
}

static void do_rom_specific_overrides() {
    static MD5_CTX md5_ctx;
    static unsigned char md5[16];

    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, (void*)prg_base, 16*1024*prg_16k_banks);
    MD5_Final(md5, &md5_ctx);

#if 0
    for (unsigned i = 0; i < 16; ++i)
        printf("%02X", md5[i]);
    putchar('\n');
#endif

    if (MEM_EQ(md5, "\xAC\x5F\x53\x53\x59\x87\x58\x45\xBC\xBD\x1B\x6F\x31\x30\x7D\xEC"))
        //* Cybernoid
        enable_bus_conflicts();
    else if (MEM_EQ(md5, "\x60\xC6\x21\xF5\xB5\x09\xD4\x14\xBB\x4A\xFB\x9B\x56\x95\xC0\x73"))
        //* High hopes
        set_pal();
    else if (MEM_EQ(md5, "\x44\x6F\xCD\x30\x75\x61\x00\xA9\x94\x35\x9A\xD4\xC5\xF8\x76\x67"))
        //* Rad Racer 2
        correct_mirroring(FOUR_SCREEN);
}

bool is_rom_loaded() {
    return rom_loaded;
}

const char* rom_filename() {
    
    return fname;
}

void set_rom_loaded(bool loaded) {
    rom_loaded = loaded;
}
