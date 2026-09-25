"""Read-only development probe for the local Mewgenics process.

No writes, injection, or save-file access. Run with the PID and module base
reported by Get-Process Mewgenics. Addresses are never used by the shipped UI.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import struct
import sys
from collections import Counter

K = C.WinDLL("kernel32", use_last_error=True)
K.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
K.OpenProcess.restype = W.HANDLE
K.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
K.ReadProcessMemory.restype = W.BOOL
K.CloseHandle.argtypes = [W.HANDLE]


class Game:
    def __init__(self, pid, base):
        self.handle = K.OpenProcess(0x1010, False, pid)
        if not self.handle:
            raise C.WinError(C.get_last_error())
        self.base = base

    def read(self, address, size):
        if not address or size < 0 or size > 1024 * 1024:
            return b""
        buf = C.create_string_buffer(size)
        count = C.c_size_t()
        if not K.ReadProcessMemory(self.handle, address, buf, size, C.byref(count)):
            return b""
        return buf.raw[:count.value]

    def number(self, address, fmt="Q"):
        b = self.read(address, struct.calcsize(fmt))
        return struct.unpack("<" + fmt, b)[0] if b else 0

    def text(self, address, wide=False):
        b = self.read(address, 32)
        if len(b) != 32:
            return ""
        length, capacity = struct.unpack_from("<QQ", b, 16)
        if length == 0 or length > 256 or capacity < length or capacity > 100000:
            return ""
        data = address if capacity < (8 if wide else 16) else struct.unpack_from("<Q", b)[0]
        raw = self.read(data, length * (2 if wide else 1))
        try:
            s = raw.decode("utf-16-le" if wide else "utf-8")
            return s if all(ch.isprintable() or ch in "\n\t" for ch in s) else ""
        except UnicodeDecodeError:
            return ""

    def typename(self, component):
        vt = self.number(component)
        locator = self.number(vt - 8)
        b = self.read(locator, 24)
        if len(b) == 24 and struct.unpack_from("<I", b)[0] == 1:
            rva = struct.unpack_from("<I", b, 12)[0]
            return self.read(self.base + rva + 16, 160).split(b"\0")[0].decode("ascii", errors="replace")
        return ""

    def scenes(self):
        director = self.number(self.base + 0x13DAC30)
        root = self.number(director + 0x28)
        begin, end = self.number(root), self.number(root + 8)
        if not begin or end < begin or end - begin > 512 * 8:
            return []
        return [(self.number(p), self.text(self.number(p) + 0x4B8)) for p in range(begin, end, 8)]

    def components(self, scene):
        vec = self.number(scene + 0x18)
        size, data = self.number(vec + 4, "I"), self.number(vec + 8)
        if size > 100000 or not data:
            return []
        raw = self.read(data, size * 8)
        return list(struct.unpack("<" + "Q" * size, raw)) if len(raw) == size * 8 else []

    def cat_data(self, cat_id):
        director = self.number(self.base + 0x13DAC30)
        manager = self.number(director + 0x598)
        h = 0xcbf29ce484222325
        for b in struct.pack("<Q", cat_id):
            h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
        mask = self.number(manager + 0x118)
        table = self.number(manager + 0x100)
        sentinel = self.number(manager + 0xf0)
        first = self.number(table + (h & mask)*16)
        node = self.number(table + (h & mask)*16 + 8)
        for _ in range(4096):
            if not node or node == sentinel:
                return 0
            if self.number(node + 0x10) == cat_id:
                return self.number(node + 0x18)
            if node == first:
                return 0
            node = self.number(node + 8)
        return 0

    def strings(self, obj, limit=0x500):
        found = []
        for off in range(0x38, limit - 31, 8):
            for wide in (False, True):
                s = self.text(obj + off, wide)
                if s:
                    found.append({"offset": hex(off), "wide": wide, "text": s})
        return found


def main():
    p = argparse.ArgumentParser()
    p.add_argument("pid", type=int)
    p.add_argument("base", type=lambda v: int(v, 0))
    p.add_argument("--dump", type=lambda v: int(v, 0))
    p.add_argument("--types", action="store_true")
    p.add_argument("--pointers", action="store_true")
    p.add_argument("--entity", action="store_true")
    p.add_argument("--cat", type=lambda v: int(v, 0))
    p.add_argument("--limit", type=lambda v: int(v, 0), default=0x300)
    args = p.parse_args()
    game = Game(args.pid, args.base)
    try:
        if args.cat is not None:
            obj = game.cat_data(args.cat)
            print(json.dumps({"cat_id": args.cat, "data": hex(obj), "strings": game.strings(obj, 0xc00), "first_bytes": game.read(obj, 0x80).hex()}, ensure_ascii=False, indent=2))
            return
        if args.dump:
            raw = game.read(args.dump, args.limit)
            print(json.dumps({"address": hex(args.dump), "type": game.typename(args.dump), "strings": game.strings(args.dump, args.limit), "words": [f"{i:03x}: {struct.unpack_from('<Q', raw, i)[0]:016x}" for i in range(0, len(raw)-7, 8)]}, ensure_ascii=False, indent=2))
            if args.pointers:
                for off in range(0x18, len(raw)-7, 8):
                    target = struct.unpack_from("<Q", raw, off)[0]
                    typ = game.typename(target)
                    strings = game.strings(target, 0x180) if typ else []
                    if typ:
                        print(json.dumps({"offset": hex(off), "target": hex(target), "type": typ, "strings": strings}, ensure_ascii=False))
            if args.entity:
                owner = game.number(args.dump + 0x18)
                size = game.number(owner + 0x24, "I")
                data = game.number(owner + 0x28)
                if 0 < size < 512:
                    for i in range(size):
                        obj = game.number(data + 8*i)
                        print(json.dumps({"entity_component": hex(obj), "type": game.typename(obj), "strings": game.strings(obj, 0x180)}, ensure_ascii=False))
            return
        scenes = game.scenes()
        print(json.dumps({"scenes": [{"ptr": hex(a), "name": n} for a, n in scenes]}, ensure_ascii=False))
        for scene, name in scenes:
            if name != "House":
                continue
            comps = game.components(scene)
            if args.types:
                print(json.dumps(Counter(game.typename(comp) for comp in comps), indent=2))
                continue
            interesting = []
            cat_count = 0
            for comp in comps:
                typ = game.typename(comp)
                if typ == ".?AVHouseCat@glaiel@@":
                    cat_count += 1
                    if cat_count > 2:
                        continue
                if any(key in typ for key in ("House", "Camera", "Zoom", "Room")):
                    interesting.append({"ptr": hex(comp), "type": typ, "strings": game.strings(comp, 0x300)})
            print(json.dumps({"component_count": len(comps), "cats": cat_count, "components": interesting}, ensure_ascii=False, indent=2))
    finally:
        K.CloseHandle(game.handle)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
