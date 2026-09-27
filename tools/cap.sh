#!/bin/bash
# cap.sh <zip-basename> <capfile> <frame> [frames] [extra shot frames]: unzip the game into
# ~/vfg/g if needed, boot it headless with the full audit's input script and write a
# GE capture (VFLASH_GECAP) starting at <frame>, plus a screenshot at that frame.
N="$1"; CAP="$2"; FR="$3"; NF=${4:-4}
Z=/mnt/c/Users/seanh/Desktop/vflashy/vtech_vflash_vsmilepro/"$N".zip
D=~/vfg/g/"$N"
if ! ls "$D"/*.cue >/dev/null 2>&1; then mkdir -p "$D"; unzip -q -o "$Z" -d "$D"; fi
CUE=$(ls "$D"/*.cue | head -1)
. "$(dirname "$0")/audit_input.sh"
IN=$(audit_input "$N" 9000)
mkdir -p ~/vfg/caps
cd ~/vfg && env $PRE VFLASH_EXIT=$((FR + NF + 2)) VFLASH_GECAP="$CAP,$FR,$NF" VFLASH_SHOT_FRAME=${5:-$FR} \
    VFLASH_SHOT=~/vfg/caps/shot%05d.ppm VFLASH_INPUT="$IN" VFLASH_GELOG=1 VFLASH_RTC=1790000000 FLASHEM_BIOS=~/rom.bin \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    ${FLASHEM:-/mnt/c/Users/seanh/Desktop/vflashy/flashem-libretro/flashem} --headless "$CUE" 2>&1 | grep -E "${GREP:-GECAP|unknown op|SCREENSHOT|exit}"
