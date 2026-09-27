#!/usr/bin/env python3
"""boottl.py <log>: one line per 60-frame report of a headless run - PC, CD
sectors / last LBA, GE lists, and the other (non-periodic) lines in between."""
import re, sys

frame, pc, events = 0, '', []
prev_sec = None
for line in open(sys.argv[1], errors='replace'):
    line = line.rstrip()
    m = re.match(r'\[HW\] frame (\d+) PC=(\w+)', line)
    if m:
        frame, pc = int(m.group(1)), m.group(2)
        continue
    m = re.match(r'\[HW\]   cd: (\d+) sectors, last lba (\d+)', line)
    if m:
        sec, lba = int(m.group(1)), int(m.group(2))
        cd = f'cd {sec:6d} (+{sec - (prev_sec or 0):4d}) lba {lba:6d}'
        prev_sec = sec
        continue
    m = re.match(r'\[HW\]   ge: (\d+) lists', line)
    if m:
        ev = ' | '.join(sorted(set(events)))[:110]
        print(f'f{frame:5d} {pc} {cd} ge {int(m.group(1)):3d}  {ev}')
        events = []
        continue
    if line.startswith(('[HW]   timer', '[Main]', '[UART0] tx', '[VE] B8')):
        continue
    events.append(line[:60])
