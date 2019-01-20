GBEmu
=====

A small experiment in writing a Game Boy emulator.

# Status
Functionally speaking the emulator is complete and reasonably accurate. Though it is definitely not feature-rich, it plays all my childhood games properly. :-)

## Known Limitations
* No save states or battery-backed SRAM support
* The timing "atom" is the single CPU instruction, so sub-instruction inter-component timing is not *exactly* right

## Known Issues
* HALT bug is not implemented
* Zombie sound envelope sweeps are not implemented
* Noise channel LFSR is not right but sounds good for the majority of games
* There is a minor sound glitch (audible in e.g. Alleyway); sound channels do not quite mute when they should
* There is a minor rendering glitch in Radar Mission Game Mode A when the game board fades in; there should be a kind of window-shade effect, but in this emulator the effect is a little janky and the screen flashes white a few times

# How to Build

Simply open the solution and build as usual. Everything should work out of the box. Both Debug Win32 and Release Win32 targets are in regular use.

The only external dependency is SDL2. This is included in the "external" folder under GBEmuNative.

# How to Use
Invoke the executable; as the first argument, specify the working directory; as the second argument, specify the name of the ROM you wish to run.

Directional pad input is mapped to cursor keys; A, B, Select and Start are mapped to P, O, Q and W, respectively.

# Goals

My goals in developing this emulator were:
* Have fun
* Clarity and correctness first, performance second
* Explore some interesting implementation techniques

## Techniques Worth Calling Out
### Template-Based CPU Opcode Subfield Parsing
A compile-time, template-based approach was used to parse CPU opcode subfields and minimize C++ code duplication. There is very little cut and paste as compared to other emulators out there, yet the generated code remains quite terse.

### Header-Based Project
Most code in this project is put in "headers". I use quotes because really these are glorified regular .cpp files, it's just that they are guarded by `#pragma once` directives. Within a given header, all functions are defined inline in their classes.

This gives two advantages:
* No need to forward-declare anything, which again cuts down on duplication.
* Without forward declarations, it is not possible to create cyclic dependencies; the dependencies between classes in the headers must form a DAG.
