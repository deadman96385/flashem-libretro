#!/usr/bin/env python3
"""auditsum.py [dir]: summarise ~/vfg/audit/<game>/ runs from tools/audit.sh.
Prints one block per game and writes <game>/sheet.png (screenshot timeline)."""
import os, re, sys, glob
from collections import Counter

root = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/vfg/audit')

def sheet(d):
    try:
        from PIL import Image
    except ImportError:
        return
    shots = sorted(glob.glob(os.path.join(d, 'f*.ppm')))
    if not shots:
        return
    ims = [Image.open(s).convert('RGB') for s in shots]
    w, h = ims[0].size
    out = Image.new('RGB', (w * 4, h * ((len(ims) + 3) // 4)))
    for i, im in enumerate(ims):
        out.paste(im, ((i % 4) * w, (i // 4) * h))
    out.save(os.path.join(d, 'sheet.png'))

def blank(path):
    """Fraction of the screenshot that is one colour."""
    try:
        from PIL import Image
    except ImportError:
        return None
    im = Image.open(path).convert('RGB')
    c = Counter(im.getdata())
    return c.most_common(1)[0][1] / (im.size[0] * im.size[1])

for d in sorted(glob.glob(os.path.join(root, '*'))):
    log = os.path.join(d, 'log.txt')
    if not os.path.exists(log):
        continue
    t = open(log, errors='replace').read()
    name = os.path.basename(d)
    banners = t.count('MORE v4.0 SDK')
    frames = [int(x) for x in re.findall(r'\[HW\] frame (\d+) PC', t)]
    fps = [int(x) for x in re.findall(r'\[Main\] (\d+) FPS', t)]
    ge = re.findall(r'ge: (\d+) lists, (\d+) sprites, (\d+) fills, (\d+) model calls, (\d+) triangles', t)
    cd = re.findall(r'cd: (\d+) sectors, last lba (\d+)', t)
    pcs = re.findall(r'\[HW\] frame \d+ PC=([0-9A-F]+)', t)
    unk = sorted(set(re.findall(r'\[GE\] unknown op ([0-9A-F]{2})', t)))
    undef = re.findall(r'Undef.*?PC=0x([0-9A-F]+)', t)
    faults = re.findall(r'translation fault VA=([0-9A-F]+) PC=([0-9A-F]+)', t)
    mmuf = t.count('[CP15] MMU fault')
    resets = t.count('soft reset via')
    wall = open(os.path.join(d, 'wall')).read().strip() if os.path.exists(os.path.join(d, 'wall')) else 'running?'
    sheet(d)
    print('==', name)
    print('   %s  last frame %s  avg fps %s  game kernel %s' % (
        wall, frames[-1] if frames else '-',
        round(sum(fps[5:]) / len(fps[5:]), 1) if len(fps) > 5 else '-',
        'started' if banners >= 2 else 'NOT started'))
    if cd:
        print('   cd: %s sectors, last lba %s' % cd[-1])
    if ge:
        print('   ge: %s lists, %s sprites, %s fills, %s model calls, %s triangles (cumulative per window)' % ge[-1])
    print('   last PCs: %s' % ' '.join(pcs[-4:]))
    if unk: print('   unknown GE ops: %s' % ' '.join(unk))
    if undef: print('   UNDEF insns: %d (first PC %s)' % (len(undef), undef[0]))
    if faults: print('   translation faults: %d (first VA %s PC %s)' % ((len(faults),) + faults[0]))
    if mmuf: print('   MMU faults: %d' % mmuf)
    if resets > 1: print('   soft resets: %d' % resets)
    shots = sorted(glob.glob(os.path.join(d, 'f*.ppm')))
    if shots:
        print('   shots: ' + ' '.join('%s:%s' % (os.path.basename(s)[1:6].lstrip('0'),
              'blank' if (blank(s) or 0) > 0.98 else 'img') for s in shots))
