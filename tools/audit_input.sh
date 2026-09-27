#!/bin/bash
# audit_input.sh: source it, then `audit_input <game name> <frames>` prints the
# VFLASH_INPUT script for that game, and `audit_shots <frames>` the shot frames.
# Default: Enter (0x100) at 2400, then every 300 frames from 3700, with a
# stick-down tap (0x2) 60 frames before every 4th press. Overrides by title,
# for games the default left stuck in a menu in the 2026-09-27 audit.

audit_input() {
    local N="$1" F="$2" IN k f every=4 hold=""
    case "$N" in
    # Cars (USA): still loading at 3700, so the taps pushed the cursor to Options
    # and every later Enter toggled Music. No taps: Enter walks Game Zone -> play.
    "Disney-Pixar Cars"*) every=0 ;;
    # Scooby-Doo: gameplay starts in a room; the default never moves Scooby.
    # Hold the stick up (0x1) and then right (0x8) in stretches once in the game.
    "Scooby-Doo"*) hold="1@6000-6600;8@6700-7200;1@7300-7900" ;;
    # SpongeBob / Shrek: long intro cutscenes (SpongeBob shows "SKIP" on green);
    # green (0x40) taps between the Enter presses reach gameplay by ~11000.
    # Multisports / Defis Sports: the event briefing starts on green ("Start").
    *"SpongeBob"*|*"Bob L'Eponge"*|*"Bob Esponja"*|"Shrek"*|"Multisports"*|"Defis Sports"*)
        hold=$(for f in $(seq 5150 300 $((F - 200))); do printf "40@%d-%d;" $f $((f + 10)); done)
        hold=${hold%;} ;;
    esac
    IN="100@2400-2410"; k=0
    for f in $(seq 3700 300 $((F - 200))); do
        k=$((k + 1))
        if [ "$every" -gt 0 ] && [ $((k % every)) -eq 0 ]; then IN="$IN;2@$((f - 60))-$((f - 50))"; fi
        IN="$IN;100@$f-$((f + 10))"
    done
    [ -n "$hold" ] && IN="$IN;$hold"
    echo "$IN"
}

# the screenshot frames that go with the default script: boot every 150 frames to
# the first press, then 5 before and 150 after every press, and before every tap
audit_shots() {
    local F="$1" SH k f
    SH="$(seq -s, 150 150 2250),2395,$(seq -s, 2550 150 3600),"
    k=0
    for f in $(seq 3700 300 $((F - 200))); do
        k=$((k + 1))
        [ $((k % 4)) -eq 0 ] && SH="$SH$((f - 65)),"
        SH="$SH$((f - 5)),$((f + 150)),"
    done
    echo "${SH%,}" | tr , ' ' | xargs -n1 | sort -n | uniq | paste -sd,
}
