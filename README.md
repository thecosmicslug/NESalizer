NESalizer for the Steam Link by TheCosmicSlug
======================================
[GPLv2]	(http://www.gnu.org/licenses/gpl-2.0.html)

A NES emulator using SDL2 originally written by 'ulfalizer' ported to the Steam Link. 
More of a Diet-Nesalizer, as alot of cool features suchh as rewind & recording were removed to allow it to run on the limited hardware of the Steamlink.

## CHANGES ##
The input system has been replaced, Gamepad support has been added. Save-States are now saved to disk, with 10 slots to change between for each ROM.
SRAM support has been added for ROMs that support it - Only tested with Legend of Zelda so far.
NES Test ROM support has been added back, improved it a little.. now it can load a list of testroms to run through.
Program arguments have been rewritten, No Arguments will launch the ROM Select Dialog. Other options are below

`./nesalizer -f "/roms/romname.nes"` - Will Load the ROM specified.

`./nesalizer -t "/testlist.txt"` - Will run through the ROMs specified in the text-file.

`./nesalizer -n` - Override ROM detection to always choose NTSC.

`./nesalizer -p` - Override ROM detection to always choose PAL.

Having finally added a method to load ROMs at runtime, I am now looking into expanding that with configurable inputs and re-add Ulf's original rewind-code now that the emulator is running at proper speed.

## THANKS ##
 * ulfalizer for the original sdl version

 * https://github.com/Kevoot/NESalizer - Kevroot ported my striped down nesalizer to the nintendo switch, which I think is awsome work! infact seeing as he has added a GUI to his port motivated me to come back to mine and see if I can learn something new cooking up a basic GUI for the steamlink version.

 * https://github.com/usrshare/nesalizerX - Another interesting port of the original nesalizer, this guy seems to know what he is doing.

 * Shay Green (blargg) for the [blip\_buf](https://code.google.com/p/blip-buf/) waveform synthesis library.
 * Quietust for the Visual 2A03 and Visual 2C02 simulators.
 * beannaich, cpow, Disch, kevtris, Movax21, Lidnariq, loopy, thefox, Tepples and lots of other people on the [NesDev](http://nesdev.com) forums and wiki and in #nesdev for help, docs, tests, and discussion.

## BUILDING ##
Requires the Steamlink-SDK available on github

git clone https://github.com/ValveSoftware/steamlink-sdk
cd steamlink-sdk/examples
git clone --recurse-submodules https://github.com/thecosmicslug/NESalizer-steamlink
cd NESalizer-steamlink
./build_nesalizer_steamlink.sh
 
## Running ##
ImGUI Support has been added, allowing for a File Open Dialog for selecting ROMs. Simply press the leftthumbstick in and the Dialog will show. A ROM filename can still be provided as program argument to load on startup.


## Controls ##

Movement:
 * SDL_CONTROLLER_BUTTON_DPAD_UP
 * SDL_CONTROLLER_BUTTON_DPAD_DOWN
 * SDL_CONTROLLER_BUTTON_DPAD_LEFT
 * SDL_CONTROLLER_BUTTON_DPAD_RIGHT

Buttons:
 * SDL_CONTROLLER_BUTTON_BACK 			= Select
 * SDL_CONTROLLER_BUTTON_START 			= Start
 * SDL_CONTROLLER_BUTTON_A			= A
 * SDL_CONTROLLER_BUTTON_B			= B

Emulation:
 * SDL_CONTROLLER_BUTTON_LEFTSHOULDER		= Load State
 * SDL_CONTROLLER_BUTTON_RIGHTSHOULDER		= Save State
 
 * SDL_CONTROLLER_BUTTON_X			= Slot -
 * SDL_CONTROLLER_BUTTON_Y			= Slot +

 * SDL_CONTROLLER_BUTTON_LEFTSTICK		= Load ROM
 * SDL_CONTROLLER_BUTTON_RIGHTSTICK		= Quit NESalizer

## Compatibility ##
iNES mappers (support circuitry inside cartridges) supported so far: 
0, 1, 2, 3, 4, 5 (including ExGrafix, split screen, and PRG RAM swapping), 7, 9, 10, 11, 13, 28, 71, 232. This covers the majority of ROMs.

Supports tricky-to-emulate games like Mig-29 Soviet Fighter, Bee 52, Uchuu Keibitai SDF, Just Breed, and Battletoads.

## Technical ##
Uses a low-level renderer that simulates the rendering pipeline in the real PPU (NES graphics processor), following the model in [this timing diagram](http://wiki.nesdev.com/w/images/d/d1/Ntsc_timing.png) that ulfalizer put together with help from the NesDev community. 

(It won't make much sense without some prior knowledge of how graphics work on the NES.

Most prediction and catch-up (two popular emulator optimization techniques) is omitted in favor of straightforward and robust code. This makes many effects that require special handling in some other emulators work automatically
