#!/bin/bash
# fullaudit.sh [zipdir] [jobs] [frames]: run every game zip headless, keep the debug
# data in ~/vfg/fullaudit/<game>/ and delete each game's extract when it is done.
# Per game: boot screenshots every 150 frames to the first press (2400), then a
# shot just before and 150 frames after every scripted Enter (and before each
# occasional stick-down), so the sheets show each screen and what a press did.
# Summaries: tools/fullsum.py.
ZIPDIR=${1:-/mnt/c/Users/seanh/Desktop/vflashy/vtech_vflash_vsmilepro}
JOBS=${2:-6}
FRAMES=${3:-13000}   # SpongeBob / Shrek / Multisports reach gameplay at ~10500-12000
OUT=${OUT:-~/vfg/fullaudit}   # OUT=, FLASHEM= (binary) override
mkdir -p "$OUT"
export OUT FRAMES FLASHEM
TOOLS=/mnt/c/Users/seanh/Desktop/vflashy/flashem-libretro/tools
export TOOLS

one() {
    Z="$1"; N=$(basename "$Z" .zip); D=~/vfg/g/"$N"; O="$OUT/$N"
    [ -f "$O/summary.txt" ] && { echo "skip (done) $N"; return; }
    mkdir -p "$O"
    if ! ls "$D"/*.cue >/dev/null 2>&1; then
        if ! unzip -tqq "$Z" >/dev/null 2>&1; then echo "BADZIP" > "$O/summary.txt"; echo "BADZIP $N"; return; fi
        mkdir -p "$D"; unzip -q -o "$Z" -d "$D" || { echo "UNZIPFAIL" > "$O/summary.txt"; return; }
    fi
    CUE=$(ls "$D"/*.cue | head -1)
    # input and screenshot frames: tools/audit_input.sh (per-title overrides there)
    . "$TOOLS/audit_input.sh"
    IN=$(audit_input "$N" "$FRAMES")
    SH=$(audit_shots "$FRAMES")
    start=$(date +%s)
    cd ~/vfg && VFLASH_EXIT=$FRAMES VFLASH_SHOT_FRAME=$SH VFLASH_SHOT="$O/f%05d.ppm" \
        VFLASH_INPUT="$IN" VFLASH_GELOG=1 VFLASH_RTC=1790000000 FLASHEM_BIOS=~/rom.bin \
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout -s KILL 1800 "${FLASHEM:-/mnt/c/Users/seanh/Desktop/vflashy/flashem-libretro/flashem}" --headless "$CUE" \
        > "$O/log.txt" 2>&1
    echo "rc=$? wall=$(( $(date +%s) - start ))" > "$O/wall"
    python3 "$TOOLS/fullsum.py" game "$O"
    # keep Dingo Rallye: boottime.sh and the GE captures use it
    case "$N" in Dingo*) ;; *) rm -rf "$D" ;; esac
    echo "done $N $(head -1 "$O/summary.txt")"
}
export -f one

ls "$ZIPDIR"/*.zip | xargs -d '\n' -P"$JOBS" -I{} bash -c 'one "$@"' _ {}
python3 "$TOOLS/fullsum.py" index "$OUT"
