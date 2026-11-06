# DJGPP Doom
Download the executables [here](https://github.com/FrenkelS/djdoom/releases).

The goal of this project is to:
* Examine which compiler generates the fastest Doom code for DOS:
  - [DJGPP](https://github.com/andrewwutw/build-djgpp)
  - [Digital Mars](https://digitalmars.com) (with [X32](https://github.com/Olde-Skuul/KitchenSink/tree/master/sdks/dos/x32))
  - [CC386](https://ladsoft.tripod.com/cc386_compiler.html)
  - [Watcom](https://github.com/open-watcom/open-watcom-v2)
* Show the modifications required in the code for it to be compilable by the compilers.

The code is based on [gamesrc-ver-recreation](https://bitbucket.org/gamesrc-ver-recreation/doom)
and it took inspiration from
[Apogee Sound System](https://bitbucket.org/gamesrc-ver-recreation/audiolib),
[Apogee Sound System backed DMX wrapper](https://bitbucket.org/gamesrc-ver-recreation/apodmx),
[doomgeneric](https://github.com/ozkl/doomgeneric),
[Doom Vanille](https://github.com/AXDOOMER/doom-vanille),
[Executor](https://github.com/ctm/executor),
[FastDoom](https://github.com/viti95/FastDoom),
[Quake](https://github.com/id-Software/Quake) and
[Quake 2](https://github.com/id-Software/Quake-2).

There is some assembly in the code that requires [NASM](https://www.nasm.us).
To build Doom using only C code, look at the macro `C_ONLY`.
To remove the visplanes limit, look at the macro `REMOVE_LIMITS`.

Sound effects are supported through the PC speaker and Sound Blaster.
And music is supported via Adlib, Sound Blaster, Pro Audio Spectrum, General MIDI, Wave Blaster and Sound Canvas.

There's no support for joystick and Logitech Cyberman.

## How to add other compilers
The differences between compilers specific to the Doom source code are in `a_blast.c`, `a_inter.h`, `a_multiv.c`, `a_taskmn.c`, `compiler.h`, `d_main.c` and `i_ibm.c`.
Search in those files for the pre-defined compiler macros and start hacking.

|Compiler    |Set environment variables|Compile code|Pre-defined compiler macro|
|------------|-------------------------|------------|--------------------------|
|DJGPP       |`setenvdj.bat`           |`compdj.bat`|`__DJGPP__`               |
|Digital Mars|`setenvdm.bat`           |`compdm.bat`|`__DMC__`                 |
|CC386       |`setenvoc.bat`           |`compoc.bat`|`__CCDL__`                |
|Watcom      |`setenvwc.bat`           |`compwc.bat`|`__WATCOMC__`             |

## DOS video backends

Build one executable with the DJGPP cross compiler:

```sh
make build_dos/doom.exe CC=~/djgpp/bin/i586-pc-msdosdjgpp-gcc \
    CXX=~/djgpp/bin/i586-pc-msdosdjgpp-g++ -j8
```

Select a backend at startup; no option defaults to `-vga`:

| Option | Destination | Presentation |
|--------|-------------|--------------|
| `-vga` | Mode Y VRAM page at A0000 + 4000h * page | Three-page VGA flip |
| `-mcga` | Internal 320x200 RAM buffer | Copy 64,000 bytes to mode 13h VRAM |
| `-vbe1` | Banked VESA VRAM window | Three-page VBE display-start flip |
| `-vbe2` | Linear VESA VRAM page | Three-page VBE display-start flip |

For example, `doom.exe -vbe2 -timedemo demo1`. If several video flags are given,
the last one wins. The selected backend is printed at startup. The Linux build
continues to use its software-plane X11 backend.

`I_GetDrawBuffer()` supplies the hidden VRAM page address and pitch before
rendering. Only MCGA uses a full-frame RAM buffer and presentation copy. VESA
and Mode Y render directly into their hidden pages, then flip without a frame
upload. Flips are immediate and do not wait for retrace. Buffered HUD/UI
rectangles still use DOOM's existing screens. Screen wipes and the disk icon
select the correct page and restore the drawing bank afterward.

The banked backend needs VBE 1.2 or newer; the LFB backend needs VBE 2.0 or newer.
Both discover a packed 320x200x8 mode through 4F00/4F01. A banked drawing page
must fit completely inside the window, with its start aligned to scanlines and
bank granularity. At 320-byte pitch and 64 KiB granularity, the pages start at
banks 0, 5 and 10 (scanlines 0, 1024 and 2048). LFB pages are contiguous and
start at scanlines 0, 200 and 400 for that pitch. Unsupported layouts fail
explicitly. VBE 2.0 palette loads use 4F09; VBE 1.2 requires a VGA-compatible
mode and uses the VGA DAC. LFB memory is mapped through DPMI and released on
exit. There is no silent fallback to another video backend.

`src/i_video.h` owns address calculation and plane selection. All four modes
share `src/scalers.asm`: columns take a destination and row stride, and spans
take a packed Y6.10:X6.10 fraction and step. Mode Y selects planes outside the
loops. Packed low detail selects a word-store variant generated from the same
macros (`mov ah, al; mov [edi], ax`); Mode Y uses byte stores with a plane mask.
Player-colour translations are precomposed with every WAD colormap, so all
columns use one colour lookup (25.5 KiB extra for the stock 34 maps).

`make test-scalers` tests the actual assembly loops on 32-bit Linux, including
sampling, composed player colours, byte/word stores, packed coordinate wrap,
negative steps, carries, plane reconstruction and all unrolled-loop tails.
It requires NASM and a C compiler with 32-bit support.

Build the standalone DOS VBE probe with the same compiler options:

```sh
make build_dos/vbe-test.exe CC=~/djgpp/bin/i586-pc-msdosdjgpp-gcc
```

Run `vbe-test.exe -vbe1` or `vbe-test.exe -vbe2`. It checks video-memory
read/write access, bank crossings, direct rendering pointers and six flips
with display-start readback and distinct contents in all three pages.
Expected output includes `VBE-DIRECT-TRIPLE-PASS` and the selected backend's
`VBE-BANKED-PASS` or `VBE-LFB-PASS` marker.
