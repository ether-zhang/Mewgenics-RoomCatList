"""Add a cat-list entry using the game's own sidebar paper and button states.

Only the center icon is new. The native openclose button supplies its paper,
state transforms and color changes. Original assets and the single ABC module
are preserved; installation of the staged file happens after the game exits.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
import zlib
from abc_patch import append_button_class


class Bits:
    def __init__(self):
        self.values = []

    def put(self, value, count):
        value &= (1 << count) - 1
        self.values.extend((value >> shift) & 1 for shift in range(count - 1, -1, -1))

    def finish(self):
        self.values.extend([0] * (-len(self.values) % 8))
        return bytes(sum(self.values[i + j] << (7 - j) for j in range(8))
                     for i in range(0, len(self.values), 8))


def signed_bits(*values):
    return max(2, *(abs(int(v)).bit_length() + 1 for v in values))


def rect(left, right, top, bottom):
    b = Bits()
    n = signed_bits(left, right, top, bottom)
    b.put(n, 5)
    for v in (left, right, top, bottom):
        b.put(v, n)
    return b.finish()


def matrix(x=0, y=0):
    b = Bits()
    b.put(0, 1)  # No scale.
    b.put(0, 1)  # No skew/rotation.
    x, y = round(x * 20), round(y * 20)
    n = signed_bits(x, y)
    b.put(n, 5)
    b.put(x, n)
    b.put(y, n)
    return b.finish()


def tag(code, body=b""):
    n = len(body)
    return struct.pack("<H", (code << 6) | min(n, 63)) + (struct.pack("<I", n) if n >= 63 else b"") + body


def polygon_shape(character, paths, colors, bounds):
    out = struct.pack("<H", character) + rect(*(round(v * 20) for v in bounds))
    out += bytes([len(colors)]) + b"".join(b"\0" + bytes(c) for c in colors) + b"\0"
    bits = Bits()
    fill_bits = max(1, len(colors).bit_length())
    bits.put(fill_bits, 4)
    bits.put(0, 4)
    for fill, points in paths:
        points = [(round(x * 20), round(y * 20)) for x, y in points]
        x, y = points[0]
        bits.put(0, 1)
        bits.put(0b00101, 5)  # MoveTo + FillStyle1 (right-hand side).
        n = signed_bits(x, y)
        bits.put(n, 5)
        bits.put(x, n)
        bits.put(y, n)
        bits.put(fill, fill_bits)
        for nx, ny in points[1:] + points[:1]:
            dx, dy = nx - x, ny - y
            n = signed_bits(dx, dy)
            if n > 17:
                raise ValueError("Shape edge exceeds SWF straight-edge capacity")
            bits.put(1, 1)
            bits.put(1, 1)
            bits.put(n - 2, 4)
            bits.put(1, 1)
            bits.put(dx, n)
            bits.put(dy, n)
            x, y = nx, ny
    bits.put(0, 6)
    return tag(32, out + bits.finish())  # DefineShape3, RGBA fills.


def place(character, depth, name, x=0, y=0):
    flags = 0x26 if name else 0x06
    body = bytes([flags]) + struct.pack("<HH", depth, character) + matrix(x, y)
    if name:
        body += name.encode("ascii") + b"\0"
    return tag(26, body)


def sprite(character, children):
    return tag(39, struct.pack("<HH", character, 1) + b"".join(children) + tag(1) + tag(0))


def circle(x, y, radius, sides=24):
    return [(x + radius * math.cos(2*math.pi*i/sides), y + radius * math.sin(2*math.pi*i/sides))
            for i in range(sides)]


def cat_list_icon(character):
    # One ink silhouette with reversed eye/nose holes. Keep the native paper
    # in a separate display object: overlapping border/ink fill contours in
    # the old shape canceled out in the engine, leaving only a tiny face.
    head=[(8,18),(32,38),(47,35),(64,38),(87,16),(90,65),(94,82),
          (90,100),(79,114),(64,123),(48,126),(28,121),(13,110),(5,94),(4,76),(8,56)]
    paths=[(1,head),(1,list(reversed(circle(31,76,5.5)))),
           (1,list(reversed(circle(66,76,5.5)))),(1,[(49,103),(56,94),(42,94)])]
    for y in (38,70,102):
        paths.append((1,circle(104,y+4,4,16)))
        paths.append((1,[(115,y),(150,y-1),(149,y+8),(114,y+9)]))
    return polygon_shape(character,paths,[(38,38,35,255)],(0,152,0,144))


def native_matrix(values):
    a,b,c,d,x,y=values;out=Bits()
    out.put((a,d)!=(1,1),1)
    if (a,d)!=(1,1):
        sx,sy=round(a*65536),round(d*65536);n=signed_bits(sx,sy)
        out.put(n,5);out.put(sx,n);out.put(sy,n)
    out.put((b,c)!=(0,0),1)
    if (b,c)!=(0,0):
        bx,cx=round(b*65536),round(c*65536);n=signed_bits(bx,cx)
        out.put(n,5);out.put(bx,n);out.put(cx,n)
    x,y=round(x),round(y);n=signed_bits(x,y)
    out.put(n,5);out.put(x,n);out.put(y,n)
    return out.finish()


def native_tint(tint):
    factors=tuple(round(v*256) for v in tint[0]);offsets=tuple(round(v) for v in tint[1])
    mult=factors!=(256,256,256,256);add=offsets!=(0,0,0,0)
    if not mult and not add:return b''
    values=(factors if mult else ())+ (offsets if add else ())
    n=signed_bits(*values);out=Bits();out.put(add,1);out.put(mult,1);out.put(n,4)
    for value in values:out.put(value,n)
    return out.finish()


def native_place(child,depth,transform,tint,name=''):
    color=native_tint(tint)
    flags=6 | (8 if color else 0) | (32 if name else 0)
    body=bytes([flags])+struct.pack('<HH',depth,child)+native_matrix(transform)+color
    if name:body+=name.encode()+b'\0'
    return tag(26,body)


def sidebar_template(entries):
    # Resolve by exported menu/node names rather than pinning character IDs.
    from build_ui_theme import NativeUi
    native=NativeUi.from_tags(entries)
    menu=native.symbols['CatMenu']
    button=next(child for child,_,_,name in native.frame(menu,index=0) if name=='openclose')
    states={state:native.frame(button,label=state) for state in ('up','over','down','disabled')}
    for children in states.values():
        if len(children)!=2 or sum(name=='icon' for _,_,_,name in children)!=1:
            raise ValueError('Unexpected native sidebar template')
    return native,button,states


DEFINITION_TAGS = {2, 6, 7, 10, 11, 14, 20, 21, 22, 32, 33, 34, 35, 36, 37,
                   39, 46, 48, 60, 75, 83, 84, 87, 91}


def tag_start(data):
    return 8 + (5 + 4 * (data[8] >> 3) + 7) // 8 + 4


def read_tags(data, start):
    p = start
    while p < len(data):
        begin = p
        if p + 2 > len(data):
            raise ValueError("Truncated SWF tag header")
        value = struct.unpack_from("<H", data, p)[0]
        p += 2
        kind, size = value >> 6, value & 63
        if size == 63:
            size = struct.unpack_from("<I", data, p)[0]
            p += 4
        if p + size > len(data):
            raise ValueError("Truncated SWF tag body")
        body = data[p:p+size]
        p += size
        yield kind, body, data[begin:p]


def symbol_entries(body):
    count = struct.unpack_from("<H", body)[0]
    p = 2
    entries = []
    for _ in range(count):
        character = struct.unpack_from("<H", body, p)[0]
        p += 2
        end = body.index(0, p)
        entries.append((character, body[p:end].decode("utf-8")))
        p = end + 1
    if p != len(body):
        raise ValueError("Unexpected trailing SymbolClass data")
    return entries


def read_base_swf():
    archive = Path(__file__).resolve().parents[3] / "resources.gpak"
    with archive.open("rb") as stream:
        count = struct.unpack("<I", stream.read(4))[0]
        total, selected = 0, None
        for _ in range(count):
            size = struct.unpack("<H", stream.read(2))[0]
            name = stream.read(size).decode("utf-8")
            length = struct.unpack("<I", stream.read(4))[0]
            if name.lower() == "swfs/house.swf":
                selected = (total, length)
            total += length
        payload = stream.tell()
        if not selected or payload + total != archive.stat().st_size:
            raise ValueError("The game archive's index could not be verified")
        stream.seek(payload + selected[0])
        data = stream.read(selected[1])
    if data[:3] == b"CWS":
        data = b"FWS" + data[3:8] + zlib.decompress(data[8:])
    if data[:3] != b"FWS" or len(data) != struct.unpack_from("<I", data, 4)[0]:
        raise ValueError("Unexpected house.swf format")
    return data


def build(base=None):
    base = read_base_swf() if base is None else base
    entries = list(read_tags(base, tag_start(base)))
    script_blocks = [body for kind, body, _ in entries if kind == 82]
    if len(script_blocks) != 1:
        raise ValueError("Mewgenics requires exactly one DoABC block in house.swf")
    script_body = script_blocks[0]
    script_offset = script_body.index(0, 4) + 1
    merged_abc, button_class = append_button_class(script_body[script_offset:])
    definitions = {struct.unpack_from("<H", body)[0]: (kind, body)
                   for kind, body, _ in entries if kind in DEFINITION_TAGS}
    symbols = [(body, symbol_entries(body)) for kind, body, _ in entries if kind == 76]
    if len(symbols) != 1:
        raise ValueError("Expected exactly one house SymbolClass table")
    matches = [character for character, name in symbols[0][1] if name == "HouseStatusUI"]
    if len(matches) != 1 or definitions[matches[0]][0] != 39:
        raise ValueError("HouseStatusUI was not found as a unique native MovieClip")
    hud_id = matches[0]
    hud = definitions[hud_id][1]
    if b"rcl_button\0" in hud:
        raise ValueError("Source asset already contains this mod's button")
    first = max(definitions) + 1
    button_id = first + 1
    if button_id > 65535:
        raise ValueError("No SWF character IDs remain")
    _,_,native_states=sidebar_template(entries)
    states=[('up','up'),('over','over'),('down','down'),('selected','down'),('disabled','disabled'),('enable','up')]
    frames = bytearray()
    for i, (name, native_state) in enumerate(states):
        frames += tag(43, name.encode("ascii") + b"\0")
        if i:
            frames += tag(28, struct.pack("<H", 1))+tag(28,struct.pack('<H',3))
        for child,transform,tint,node_name in native_states[native_state]:
            frames+=native_place(first if node_name=='icon' else child,3 if node_name=='icon' else 1,transform,tint,node_name)
        frames += tag(1)
    button = tag(39, struct.pack("<HH", button_id, len(states)) + frames + tag(0))
    depths = []
    for kind, body, _ in read_tags(hud, 4):
        offset = {4: 2, 5: 2, 26: 1, 28: 0, 70: 2}.get(kind)
        if offset is not None:
            depths.append(struct.unpack_from("<H", body, offset)[0])
    depth = max(depths, default=0) + 1
    changed_hud = bytearray(hud[:4])
    added = False
    for kind, body, raw in read_tags(hud, 4):
        if kind == 1 and not added:
            changed_hud += place(button_id, depth, "rcl_button", 10, 484)
            added = True
        changed_hud += raw
    if not added:
        raise ValueError("House HUD has no initial frame")
    output = bytearray(base[:tag_start(base)])
    for kind, body, raw in entries:
        if kind == 39 and struct.unpack_from("<H", body)[0] == hud_id:
            output += cat_list_icon(first) + button + tag(39, changed_hud)
        elif kind == 82:
            output += tag(82, script_body[:script_offset] + merged_abc)
        elif kind == 76:
            count = struct.unpack_from("<H", body)[0]
            output += tag(76, struct.pack("<H", count+1) + body[2:] + struct.pack("<H", button_id) + button_class.encode("utf-8") + b"\0")
        else:
            output += raw
    struct.pack_into("<I", output, 4, len(output))
    if sum(kind == 82 for kind, _, _ in read_tags(output, tag_start(output))) != 1:
        raise ValueError("Generated SWF violated the single-DoABC engine constraint")
    return bytes(output)


if __name__ == "__main__":
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=root/'build/swfs/house.swf')
    destination=parser.parse_args().output
    destination.parent.mkdir(parents=True, exist_ok=True)
    original = read_base_swf()
    result = build(original)
    destination.write_bytes(result)
    report = {"source_sha256": hashlib.sha256(original).hexdigest(), "result_sha256": hashlib.sha256(result).hexdigest(),
              "source_bytes": len(original), "result_bytes": len(result), "added_bytes": len(result)-len(original),
              "doabc_blocks": sum(kind == 82 for kind, _, _ in read_tags(result, tag_start(result)))}
    report_file = root / "build" / "native_asset_report.json"
    report_file.parent.mkdir(parents=True, exist_ok=True)
    report_file.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Native HUD patch: {destination.name}; {report['added_bytes']} bytes added; original GPAK untouched.")
