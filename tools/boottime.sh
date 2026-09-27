#!/bin/bash
# boottime.sh <outdir> [exit frame] [shot frame]: headless Dingo Rallye boot,
# timed; log in <outdir>/log.txt, screenshot in <outdir>/menu.ppm.
# Extra VFLASH_* settings pass through the environment.
OUT=~/vfg/${1:-base}; EXIT=${2:-3700}; SHOT=${3:-3650}
mkdir -p "$OUT"; cd ~/vfg
start=$(date +%s.%N)
VFLASH_EXIT=$EXIT VFLASH_SHOT_FRAME=$SHOT VFLASH_SHOT="$OUT/menu.ppm" FLASHEM_BIOS=~/rom.bin \
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  /mnt/c/Users/seanh/Desktop/vflashy/flashem-libretro/flashem --headless \
  "g/Dingo Rallye - Fou! Fou! Fou! (France)/Dingo Rallye - Fou! Fou! Fou! (France).cue" > "$OUT/log.txt" 2>&1
end=$(date +%s.%N)
echo "wall: $(echo "$end - $start" | bc) s"
grep "FPS" "$OUT/log.txt" | awk '{s+=$2; n++} END {if (n) print "avg fps:", s/n}'
