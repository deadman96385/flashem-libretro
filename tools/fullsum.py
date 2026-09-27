#!/usr/bin/env python3
"""fullsum.py game <dir> | index <root>: summaries for tools/fullaudit.sh.

game:  parse <dir>/log.txt, convert f*.ppm to PNG, build boot_sheet.png (frames
       <= 3650) and menu_sheet.png (before/after pairs round each press), write summary.txt.
index: collect every <root>/*/summary.txt into <root>/index.csv."""
import csv, glob, os, re, sys
from collections import Counter

try:
    from PIL import Image
except ImportError:
    Image = None


def blank(im):
    c = Counter(im.getdata())
    return c.most_common(1)[0][1] / (im.size[0] * im.size[1]) > 0.98


def noisy(im):
    """Garbage like Multisports' raw sector buffer: neighbouring pixels differ
    wildly almost everywhere (real frames, even detailed ones, stay far below)."""
    import numpy as np
    a = np.asarray(im.convert('RGB'), dtype=np.int16)
    # share of pixels far from their right neighbour: Multisports' noise 0.365,
    # the busiest real frame of the 2026-09-27 audit (Cars' loading art) 0.154
    return (np.abs(a[:, 1:] - a[:, :-1]).sum(axis=2) > 120).mean() > 0.25


def sheet(paths, out, cols=6, scale=2):
    if not paths or not Image:
        return
    ims = [Image.open(p).convert('RGB') for p in paths]
    w, h = ims[0].size
    w, h = w // scale, h // scale
    o = Image.new('RGB', (w * cols, h * ((len(ims) + cols - 1) // cols)))
    for i, im in enumerate(ims):
        o.paste(im.resize((w, h)), ((i % cols) * w, (i // cols) * h))
    o.save(out)


def game(d):
    t = open(os.path.join(d, 'log.txt'), errors='replace').read()
    wall = open(os.path.join(d, 'wall')).read().strip() if os.path.exists(os.path.join(d, 'wall')) else ''
    shots = []
    for p in sorted(glob.glob(os.path.join(d, 'f*.ppm'))):
        q = p[:-4] + '.png'
        if Image:
            Image.open(p).save(q)
            os.remove(p)
        shots.append(q)
    shots = sorted(glob.glob(os.path.join(d, 'f*.png')))
    fr = lambda p: int(os.path.basename(p)[1:6])
    sheet([p for p in shots if fr(p) <= 3650], os.path.join(d, 'boot_sheet.png'))
    sheet([p for p in shots if fr(p) > 3650], os.path.join(d, 'menu_sheet.png'), cols=2)
    blanks = [fr(p) for p in shots if Image and blank(Image.open(p).convert('RGB'))]
    noise = [fr(p) for p in shots if Image and noisy(Image.open(p).convert('RGB'))]
    runaway = len(re.findall(r'\[GE\] runaway list', t))
    unk_n = Counter()
    for line in re.findall(r'\[HW\]   ge unknown:([^\n]*)', t):
        for op, n in re.findall(r'([0-9A-F]{2}):(\d+)', line):
            unk_n[op] += int(n)
    frames = [int(x) for x in re.findall(r'\[HW\] frame (\d+) PC', t)]
    fps = [int(x) for x in re.findall(r'\[Main\] (\d+) FPS', t)]
    cd = re.findall(r'cd: (\d+) sectors, last lba (\d+)', t)
    ge = re.findall(r'ge: (\d+) lists, (\d+) sprites, (\d+) fills, (\d+) model calls, (\d+) triangles', t)
    pcs = re.findall(r'\[HW\] frame \d+ PC=([0-9A-F]+)', t)
    unk = sorted(set(re.findall(r'\[GE\] unknown op ([0-9A-F]{2})', t)))
    kernel = t.count('MORE v4.0 SDK') >= 2
    undef = len(re.findall(r'Undef', t))
    faults = len(re.findall(r'translation fault|MMU fault', t))
    resets = t.count('soft reset via')
    asserts = re.findall(r'ASSERT[^\n]*', t)
    # the game is still drawing at the end if the last report shows GE lists
    drawing = bool(ge) and int(ge[-1][0]) > 0
    blank_end = len(shots) >= 3 and all(fr(p) in blanks for p in shots[-3:])
    noise_end = len(shots) >= 3 and all(fr(p) in noise for p in shots[-3:])
    status = ('OK' if kernel and drawing and not blank_end and not noise_end else
              'NO-KERNEL' if not kernel else 'NOT-DRAWING' if not drawing else
              'BLANK-END' if blank_end else 'NOISE-END')
    s = {
        'status': status, 'wall': wall, 'last_frame': frames[-1] if frames else 0,
        'avg_fps': round(sum(fps[5:]) / len(fps[5:]), 1) if len(fps) > 5 else 0,
        'kernel': kernel, 'cd_sectors': cd[-1][0] if cd else 0, 'last_lba': cd[-1][1] if cd else 0,
        'ge_lists': ge[-1][0] if ge else 0, 'triangles': ge[-1][4] if ge else 0,
        'unknown_ops': ' '.join(unk), 'last_pcs': ' '.join(pcs[-3:]), 'undef': undef,
        'faults': faults, 'soft_resets': resets, 'asserts': len(asserts),
        'blank_frames': ' '.join(map(str, blanks)), 'noise_frames': ' '.join(map(str, noise)),
        'runaway_lists': runaway,
        'unknown_counts': ' '.join('%s:%d' % kv for kv in sorted(unk_n.items())),
        'shots': len(shots),
    }
    with open(os.path.join(d, 'summary.txt'), 'w') as f:
        f.write(status + '\n')
        for k, v in s.items():
            f.write('%s: %s\n' % (k, v))
        for a in asserts[:5]:
            f.write('assert: %s\n' % a)


def index(root):
    rows = []
    for p in sorted(glob.glob(os.path.join(root, '*', 'summary.txt'))):
        lines = open(p).read().splitlines()
        r = {'game': os.path.basename(os.path.dirname(p)), 'status': lines[0] if lines else ''}
        for l in lines[1:]:
            if ': ' in l and not l.startswith('assert: '):
                k, v = l.split(': ', 1)
                r[k] = v
        rows.append(r)
    keys = ['game', 'status'] + [k for k in (rows[0].keys() if rows else []) if k not in ('game', 'status')]
    keys = list(dict.fromkeys(keys + [k for r in rows for k in r]))
    with open(os.path.join(root, 'index.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)
    print(Counter(r['status'] for r in rows))


if __name__ == '__main__':
    {'game': game, 'index': index}[sys.argv[1]](sys.argv[2])
