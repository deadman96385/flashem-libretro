# FlashEm

FlashEm is an emulator for the VTech V.Flash (V.Smile Pro) educational console (2006).

It emulates the hardware and runs the console's own boot ROM: the ARM core
boots `70004.bin`, the µMORE kernel starts, and everything on screen is what
that code draws through the emulated video engine. There is no high-level
emulation of the operating system or the games.

## Status

- Boots the real ROM through SDRAM calibration, the power-on check, the warm
  reset and the kernel, which prints its banner on the first UART.
- Plays the VTech and V.Smile Pro logos and reaches the system menu, rendered
  by the video engine model (tile layers, colour layers, palette, scroll).
- **Games do not run yet.** The CD drive is not emulated: the system menu asks
  for a disc. The V.Flash drives the CD servo and decoder directly (Vitec's
  "MAIKO" driver), and every game's `BOOT.BIN` brings its own copy of that
  driver, so the drive has to be emulated at the hardware level.
- No sound output yet, no save states.

## Hardware

| Component | Details |
|-----------|---------|
| SoC | LSI Logic ZEVIO 1020 (the TI-Nspire Classic's chip), ARM926EJ-S @ 150MHz |
| RAM | 16MB SDRAM |
| Media | CD-ROM, ISO 9660, no copy protection |
| Video | Video engine at 0xB8000000: 4 tile layers + 2 BGR555 layers, PAL/NTSC |
| OS | µMORE v4.0 RTOS (ACCESS Co.) |
| Boot ROM | 70004.bin (2MB, required) |

What is known about each device, and how it was worked out, is written next to
its model in [src/hw.c](src/hw.c). Firebird (the TI-Nspire emulator) is the
reference for the parts the two machines share; where the V.Flash differs, the
comments say so.

| Address | Device |
|---------|--------|
| 0x00000000 | Boot ROM |
| 0x10000000 | SDRAM (16MB) |
| 0x8FFF0000 | SDRAM controller |
| 0x90000000, 0x900D0000 | GPIO banks A and B (B0 bit 0: powered on) |
| 0x90010000, 0x900C0000 | Timer pairs (IRQ lines 5 and 6) |
| 0x90020000, 0x90030000 | UARTs |
| 0x90090000 | RTC, scratch registers kept across a reset |
| 0x900A0000 | Misc: reset, boot status, timer interrupts, keys (0x18) |
| 0x900B0000 | PMU / clocks |
| 0xA1000000 | SPI master (no device attached yet) |
| 0xA8000000 | Display output controller |
| 0xB8000000 | Video engine; palette RAM at 0xB8000800 (IRQ line 21) |
| 0xC4000000 | CD servo bus and CD-ROM decoder (not emulated yet) |
| 0xDC000000 | Interrupt controller |
| 0xF8000000 | 8KB shared RAM |

## Controls

| Key | V.Flash |
|-----|---------|
| Arrow keys | Up / Down / Left / Right |
| Z / X / C / V | Red / Yellow / Green / Blue |
| Enter | Enter |
| F2 | Pause / resume debugger |
| F5 | Save screenshot (BMP) |
| F11 | Toggle fullscreen |
| Esc | Quit |

Which bits of the key register (0x900A0018) belong to which button is only
partly known; see `hw_keys()` in src/hw.c.

## libretro core

```bash
make -f Makefile.libretro platform=unix   # flashem_libretro.so
```

The core is the emulator without the SDL frontend: `vflash_run_frame()` once per
`retro_run`, and the framebuffer handed over as XRGB8888 at 4:3 - 320x240, or
320x288 / 352x288 on a PAL system, as the video engine is programmed. It takes
`.cue`, `.bin` and `.iso` by path (`need_fullpath`), because the emulator opens
the disc itself. Put `70004.bin` in the frontend's system directory.

| RetroPad | V.Flash |
|----------|---------|
| D-pad | Up / Down / Left / Right |
| A | Red |
| B | Yellow |
| X | Green |
| Y | Blue |
| Start | Enter |

Save states, rewind, run-ahead and netplay are out until the emulator grows a
serialiser.

## Build

```bash
sudo apt install libsdl2-dev
make
```

The core needs only a C compiler and libm.

## Run

```bash
./flashem game.cue                  # BIN/CUE image
./flashem game.iso                  # ISO image
./flashem --dbg game.cue            # debugger, paused
./flashem --dbg-run game.cue        # debugger, running
./flashem --headless game.cue       # no display
./flashem --scale 3 game.cue        # 3x window
```

The boot ROM `70004.bin` is looked for in this order:

1. the frontend's system directory, as `<system>/flashem/70004.bin` or `<system>/70004.bin` (libretro core)
2. `$FLASHEM_BIOS`, a full path to the file
3. `70004.bin` in the current directory

### Diagnostics

| Variable | Effect |
|----------|--------|
| `VFLASH_LOG=<file>` | Send stdout (device log, `[HW?]` unmodelled accesses, UART text) to a file |
| `VFLASH_SHOT_FRAME=N` | Headless: save `/tmp/vflash_screen.ppm` at frame N (default 50) |
| `VFLASH_RAMDUMP=<file>` | Dump SDRAM (and `<file>.sram`, the palette RAM) at `VFLASH_RAMDUMP_FRAME` (default 600) |
| `VFLASH_TRACEPC=<hex>[,N]` | Print registers the first N times the instruction at that address runs |
| `VFLASH_IOHIST=<from>,<to>` | Histogram of device accesses by address and PC over a frame range |
| `VFLASH_KEYS=<hex>@<from>-<to>` | Hold raw key-register bits over a frame range |

## Debugger

```
s [N]      step N instructions    c         continue
n          step over              b <addr>  set breakpoint
bc <a>|all clear BP(s)           bl        list BPs
r          registers              pc        current instruction
d <a> [N]  disassemble            m <a> [N] memory dump
bt         stack dump              setreg r0=val
q          quit
```

## Tools

```bash
./disc_analyze  game.iso           # list disc contents
./disc_compare  g1.iso g2.iso      # compare disc structures
./mjp_extract   game.iso frames/   # extract MJP video frames
./ptx_extract   game.iso images/   # extract PTX images
```

## License

MIT — see [LICENSE](LICENSE). `src/libretro.h` comes from the libretro project
and keeps its own MIT notice.
