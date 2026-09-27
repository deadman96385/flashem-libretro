#!/bin/bash
# audit.sh <zip> [exit frame]: unpack a game into ~/vfg/g, boot it headless with
# the ROM, take a screenshot timeline and press Enter twice late in the run.
# Output: ~/vfg/audit/<name>/{log.txt,f<frame>.ppm,wall}
Z="$1"; EXIT=${2:-6000}; N=$(basename "$Z" .zip)
D=~/vfg/g/"$N"; OUT=~/vfg/audit/"$N"; mkdir -p "$D" "$OUT"
ls "$D"/*.cue >/dev/null 2>&1 || unzip -q -o "$Z" -d "$D" || { echo "UNZIP FAIL $N"; exit 1; }
CUE=$(ls "$D"/*.cue | head -1)
cd ~/vfg; rm -f "$OUT"/*.ppm
start=$(date +%s)
VFLASH_EXIT=$EXIT VFLASH_SHOT_FRAME=${SHOTS:-900,1800,2700,3600,4100,4600,5400,5990} \
VFLASH_SHOT="$OUT/f%05d.ppm" VFLASH_INPUT=${INPUT:-"100@4200-4210;100@5000-5010"} \
VFLASH_GELOG=1 FLASHEM_BIOS=~/rom.bin SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout -s KILL 1200 /mnt/c/Users/seanh/Desktop/vflashy/flashem-libretro/flashem --headless "$CUE" > "$OUT/log.txt" 2>&1
echo "rc=$? wall=$(( $(date +%s) - start ))" > "$OUT/wall"
