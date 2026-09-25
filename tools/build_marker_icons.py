"""Rasterize the game's 19 player-tag vectors into the list's existing font atlas.

Reads only the installed archive. Generated game pixels are not tracked in Git.
Uses nonzero winding per fill, preserving holes and quadratic SWF outlines.
"""
from pathlib import Path
import math
import struct
import zlib
import csv
import io
import json
from PIL import Image, ImageDraw
from game_archive import GameArchive
from build_sidebar_swf import read_tags, tag_start, symbol_entries

ROOT = Path(__file__).resolve().parents[1]
MARKERS = ('star2 circle triangle square sword shield2 poop str dex con spd int cha lck '
           'stimulation comfort appeal health evolution').split()
SIZE, SCALE = 24, 4
CLASSES = ('Fighter Mage Hunter Tank Thief Cleric Necromancer Psychic Druid '
           'Tinkerer Monk Butcher Jester Colorless').split()


class Reader:
    def __init__(self, data, byte=0):
        self.data, self.bit = data, byte * 8

    def u(self, n):
        result = 0
        for _ in range(n):
            result = result * 2 + ((self.data[self.bit // 8] >> (7 - self.bit % 8)) & 1)
            self.bit += 1
        return result

    def s(self, n):
        v = self.u(n)
        return v - (1 << n) if n and v & (1 << (n - 1)) else v

    def align(self):
        self.bit = (self.bit + 7) // 8 * 8

    def byte(self):
        self.align()
        return self.u(8)

    def word(self):
        return self.byte() | self.byte() << 8


def matrix(r):
    r.align()
    a = d = 1.0
    b = c = 0.0
    if r.u(1):
        n = r.u(5)
        a, d = r.s(n) / 65536, r.s(n) / 65536
    if r.u(1):
        n = r.u(5)
        b, c = r.s(n) / 65536, r.s(n) / 65536
    n = r.u(5)
    x, y = r.s(n), r.s(n)
    r.align()
    return a, b, c, d, x, y


def point(m, p):
    a, b, c, d, x, y = m
    return a*p[0] + c*p[1] + x, b*p[0] + d*p[1] + y


def compose(a, b):
    origin = point(a, (b[4], b[5]))
    return (a[0]*b[0]+a[2]*b[1], a[1]*b[0]+a[3]*b[1],
            a[0]*b[2]+a[2]*b[3], a[1]*b[2]+a[3]*b[3], *origin)


def styles(r, kind):
    count = r.byte()
    if count == 255 and kind != 2:
        count = r.word()
    for _ in range(count):
        fill = r.byte()
        if fill == 0:
            for _ in range(4 if kind == 32 else 3): r.byte()
        elif fill in (0x10, 0x12, 0x13):
            matrix(r)
            gradients = r.byte() & 15
            for _ in range(gradients * (5 if kind == 32 else 4)): r.byte()
            if fill == 0x13: r.word()
        else:
            raise ValueError(f'Unexpected bitmap fill in player icon: {fill}')
    lines = r.byte()
    if lines == 255 and kind != 2: lines = r.word()
    for _ in range(lines):
        r.word()
        for _ in range(4 if kind == 32 else 3): r.byte()
    return count


def shape(kind, data):
    if kind not in (2, 22, 32): raise ValueError('Unsupported player-icon shape')
    r = Reader(data, 2)
    n = r.u(5)
    for _ in range(4): r.s(n)
    count = styles(r, kind)
    fill_bits, line_bits = r.u(4), r.u(4)
    groups = [[] for _ in range(count)]
    fills = [0, 0]
    offset = 0
    pos = (0, 0)
    while True:
        if r.u(1):
            straight, n = r.u(1), r.u(4) + 2
            start = pos
            if straight:
                if r.u(1): dx, dy = r.s(n), r.s(n)
                elif r.u(1): dx, dy = 0, r.s(n)
                else: dx, dy = r.s(n), 0
                pos = start[0]+dx, start[1]+dy
                points = [start, pos]
            else:
                control = start[0]+r.s(n), start[1]+r.s(n)
                pos = control[0]+r.s(n), control[1]+r.s(n)
                points = []
                for i in range(13):
                    t = i/12
                    points.append(tuple((1-t)**2*start[j]+2*(1-t)*t*control[j]+t*t*pos[j] for j in (0, 1)))
            for i, f in enumerate(fills):
                if f:
                    for a, b in zip(points, points[1:]):
                        groups[offset+f-1].append((a, b) if i else (b, a))
        else:
            flags = r.u(5)
            if not flags: break
            if flags & 1:
                n = r.u(5)
                pos = r.s(n), r.s(n)
            if flags & 2: fills[0] = r.u(fill_bits)
            if flags & 4: fills[1] = r.u(fill_bits)
            if flags & 8: r.u(line_bits)
            if flags & 16:
                offset = len(groups)
                groups.extend([] for _ in range(styles(r, kind)))
                fill_bits, line_bits = r.u(4), r.u(4)
    return [g for g in groups if g]


def collect(defs, character, transform=(1, 0, 0, 1, 0, 0), depth=0):
    if depth > 8: raise ValueError('Cyclic player-icon sprite')
    kind, data = defs[character]
    if kind != 39:
        return [[(point(transform, a), point(transform, b)) for a, b in g] for g in shape(kind, data)]
    result = []
    for code, body, _ in read_tags(data, 4):
        if code == 1: break  # Player tag icons are static, first frame only.
        if code != 26: continue
        r = Reader(body)
        flags = r.byte()
        r.word()
        if not flags & 2: raise ValueError('Unexpected player-icon timeline update')
        child = r.word()
        m = matrix(r) if flags & 4 else (1, 0, 0, 1, 0, 0)
        result.extend(collect(defs, child, compose(transform, m), depth+1))
    return result


def raster(groups):
    pts = [p for g in groups for edge in g for p in edge]
    left, top = min(p[0] for p in pts), min(p[1] for p in pts)
    right, bottom = max(p[0] for p in pts), max(p[1] for p in pts)
    size = SIZE*SCALE
    factor = (size-2*SCALE) / max(right-left, bottom-top)
    dx = (size-(right-left)*factor)/2-left*factor
    dy = (size-(bottom-top)*factor)/2-top*factor
    img = Image.new('L', (size, size))
    draw = ImageDraw.Draw(img)
    for group in groups:
        edges = [((a[0]*factor+dx, a[1]*factor+dy), (b[0]*factor+dx, b[1]*factor+dy)) for a,b in group]
        for y in range(size):
            crossings = []
            for a, b in edges:
                if min(a[1], b[1]) <= y+.5 < max(a[1], b[1]):
                    x = a[0] + (y+.5-a[1])*(b[0]-a[0])/(b[1]-a[1])
                    crossings.append((x, 1 if b[1]>a[1] else -1))
            winding, previous = 0, 0
            for x, delta in sorted(crossings):
                if winding:
                    x0, x1 = math.ceil(previous-.5), math.floor(x-.5)
                    if x0 <= x1: draw.line((x0,y,x1,y), fill=255)
                winding += delta
                previous = x
    return img.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def build():
    archive = GameArchive(ROOT.parents[1] / 'resources.gpak')
    offset, size = archive.entries['swfs/ui.swf']
    with archive.path.open('rb') as stream:
        stream.seek(archive.base + offset)
        data = stream.read(size)
    if data[:3] == b'CWS': data = b'FWS'+data[3:8]+zlib.decompress(data[8:])
    tags = list(read_tags(data, tag_start(data)))
    defs = {struct.unpack_from('<H', p)[0]:(k,p) for k,p,_ in tags if k in (2,22,32,39,83)}
    symbols = {name.lower()[9:]:i for k,p,_ in tags if k == 76
               for i,name in symbol_entries(p) if name.lower().startswith('fonticon_')}
    icons = [(name, raster(collect(defs, symbols[name]))) for name in MARKERS]
    labels = {row['KEY']: (row['zh-cn'].replace('\u200b',''), row['en'].replace('\u200b',''))
              for row in csv.DictReader(io.StringIO(archive.text('data/text/combined.csv')))}
    classes = [(name, *labels['CAT_CLASS_'+('MEDIC' if name == 'Cleric' else name.upper())+'_NAME'],
                raster(collect(defs,symbols[name.lower()]))) for name in CLASSES]
    assert all(max(icon.tobytes()) == 255 and min(icon.tobytes()) < 8 for _, icon in icons)
    assert all(max(icon.tobytes()) == 255 and min(icon.tobytes()) < 8 for _,_,_,icon in classes)
    lines = ['// Generated from local ui.swf. Do not redistribute as source.', '#pragma once',
             'struct MarkerIcon { const char* name; unsigned char alpha[24*24]; };',
             'inline constexpr MarkerIcon kMarkerIcons[] = {']
    for name, icon in icons:
        lines.append('    {"%s", {%s}},' % (name, ','.join(str(x) for x in icon.tobytes())))
    lines += ['};', 'struct ClassIcon { const char* name; const char* label; const char* label_en; unsigned char alpha[24*24]; };',
              'inline constexpr ClassIcon kClassIcons[] = {']
    for name,label,label_en,icon in classes:
        lines.append('    {%s, %s, %s, {%s}},' % (json.dumps(name),json.dumps(label,ensure_ascii=False),
                     json.dumps(label_en,ensure_ascii=False),','.join(str(x) for x in icon.tobytes())))
    lines += ['};', '']
    (ROOT/'src/marker_icons.generated.hpp').write_text('\n'.join(lines), encoding='utf-8')
    preview = Image.new('RGB', (len(icons)*40, 48), '#242424')
    for i, (_, icon) in enumerate(icons): preview.paste('white', (i*40+8,12,i*40+32,36), icon)
    preview.save(ROOT/'build/marker-icons-preview.png')
    preview = Image.new('RGB',(len(classes)*40,48),'#242424')
    for i,(_,_,_,icon) in enumerate(classes): preview.paste('white',(i*40+8,12,i*40+32,36),icon)
    preview.save(ROOT/'build/class-icons-preview.png')
    print(f'Player markers verified: {len(icons)} original vectors, cached font-atlas glyphs.')
    print(f'Classes verified: {len(classes)} original font icons and localized names.')
    return icons


if __name__ == '__main__':
    build()
