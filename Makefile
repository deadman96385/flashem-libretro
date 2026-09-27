# Standalone emulator (SDL2 frontend) and the tools.
#
#   make                  Linux / WSL: flashem, with the system SDL2 (sdl2-config)
#   make platform=win     Windows: flashem.exe + SDL2.dll, cross-compiled with MinGW-w64
#                         from Linux/WSL, or built natively in an MSYS2 MinGW64 shell
#
# Windows needs the SDL2 MinGW development package (SDL2-devel-2.x-mingw.tar.gz from
# https://github.com/libsdl-org/SDL/releases). Point SDL2_PREFIX at its
# x86_64-w64-mingw32 directory; in MSYS2 leave it empty to use the installed
# mingw-w64-x86_64-SDL2 (sdl2-config). Windows objects are *.win.o, so the two
# builds can share the tree.

ifeq ($(OS),Windows_NT)
	platform ?= win
endif

SRCS = src/main.c src/vflash.c src/hw.c src/arm9.c src/cp15.c src/cdrom.c src/cdsp.c src/ge.c \
       src/audio.c src/disasm.c src/debugger.c src/zevio_dsp.c src/zsp400.c src/midi.c src/cdda_dma.c

ifeq ($(platform),win)
	# cross prefix unless we are on Windows already (MSYS2 has a plain gcc)
	ifeq ($(OS),Windows_NT)
		CROSS ?=
	else
		CROSS ?= x86_64-w64-mingw32-
	endif
	CC      = $(CROSS)gcc
	SDL2_PREFIX ?= $(HOME)/vfg/deps/SDL2-2.32.10/x86_64-w64-mingw32
	ifneq ($(wildcard $(SDL2_PREFIX)/include/SDL2/SDL.h),)
		SDL_CFLAGS = -I$(SDL2_PREFIX)/include -I$(SDL2_PREFIX)/include/SDL2 -Dmain=SDL_main
		SDL_LIBS   = -L$(SDL2_PREFIX)/lib -lmingw32 -lSDL2main -lSDL2
		SDL_DLL    = $(SDL2_PREFIX)/bin/SDL2.dll
	else
		SDL_CFLAGS = $(shell sdl2-config --cflags)
		SDL_LIBS   = $(shell sdl2-config --libs)
		SDL_DLL    =
	endif
	O     = .win.o
	EXE   = .exe
	# -mconsole: keep the console, the emulator logs to stdout
	LDFLAGS += $(SDL_LIBS) -lm -mconsole -static-libgcc
else
	CC      = gcc
	SDL_CFLAGS = $(shell sdl2-config --cflags)
	O     = .o
	EXE   =
	LDFLAGS += $(shell sdl2-config --libs) -lm
endif

CFLAGS += -O2 -Wall -Wextra -Iinclude $(SDL_CFLAGS)
OBJS    = $(SRCS:.c=$(O))
BIN     = flashem$(EXE)

TOOLS = gereplay$(EXE) disc_analyze$(EXE) mjp_extract$(EXE) ptx_extract$(EXE) disc_compare$(EXE) testrom_gen$(EXE)

all: $(BIN) $(TOOLS)

src/hw$(O): src/zevio_dsp.h src/zsp400.h src/midi.h src/cdda_dma.h src/cdsp.h
src/cdsp$(O): src/cdsp.h
src/zevio_dsp$(O): src/zevio_dsp.h src/zsp400.h
src/zsp400$(O): src/zsp400.h
src/midi$(O): src/midi.h
src/cdda_dma$(O): src/cdda_dma.h
src/audio$(O) src/vflash$(O) src/main$(O): src/audio.h
src/main$(O): src/frame_pacer.h

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)
ifneq ($(SDL_DLL),)
	cp $(SDL_DLL) .
endif

testrom_gen$(EXE): testrom_gen.c
	$(CC) -O2 -Wall testrom_gen.c -o $@

testrom.bin: testrom_gen$(EXE)
	./testrom_gen$(EXE)

# builds ge.c itself, so it never links a stale ge.o
gereplay$(EXE): tools/gereplay.c src/ge.c src/ge.h
	$(CC) -O2 -Wall -Wextra tools/gereplay.c src/ge.c -o $@ -lm

disc_analyze$(EXE): src/disc_analyze.c
	$(CC) -O2 -Wall src/disc_analyze.c -o $@

mjp_extract$(EXE): src/mjp_extract.c
	$(CC) -O2 -Wall src/mjp_extract.c -o $@

ptx_extract$(EXE): src/ptx_extract.c src/ptx.c
	$(CC) -O2 -Wall -Isrc src/ptx_extract.c src/ptx.c -o $@

disc_compare$(EXE): src/disc_compare.c
	$(CC) -O2 -Wall src/disc_compare.c -o $@

%$(O): %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN) $(TOOLS) testrom.bin

.PHONY: all clean
