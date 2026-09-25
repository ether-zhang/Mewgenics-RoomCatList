"""Append a native button class to the game's single ABC module.

Existing pool indexes, classes, scripts, method records and method bytecode
remain unchanged. New code follows a verified stop-only class already emitted
by the game's compiler, including its original lexical scope chain.
"""
from dataclasses import dataclass
import struct


def uint(value):
    if not 0 <= value <= 0xFFFFFFFF:
        raise ValueError("ABC integer out of range")
    out = bytearray()
    while value >= 128:
        out.append((value & 127) | 128)
        value >>= 7
    return bytes(out + bytes([value]))


class Cursor:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def read(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise ValueError("Truncated ABC record")
        result = self.data[self.pos:self.pos+size]
        self.pos += size
        return result

    def byte(self):
        return self.read(1)[0]

    def number(self):
        result = 0
        for shift in range(0, 35, 7):
            part = self.byte()
            result |= (part & 127) << shift
            if part < 128:
                return result
        raise ValueError("Invalid ABC integer")

    def traits(self):
        result = []
        for _ in range(self.number()):
            name, flags = self.number(), self.byte()
            kind = flags & 15
            slot, target = self.number(), self.number()
            if kind in (0, 6):
                if self.number():
                    self.byte()
            elif kind not in (1, 2, 3, 4, 5):
                raise ValueError(f"Unsupported ABC trait {kind}")
            if flags & 64:
                for _ in range(self.number()):
                    self.number()
            result.append((name, kind, slot, target))
        return result


@dataclass
class Table:
    count: int
    records: list
    values: list
    implicit: bool = False

    def with_extra(self, extra):
        count = max(1, self.count) if self.implicit and extra else self.count
        return uint(count + len(extra)) + b"".join(self.records + extra)


class AbcModule:
    POOLS = ("integers", "unsigned", "doubles", "strings", "namespaces", "namespace_sets", "names")

    def __init__(self, data):
        c = Cursor(data)
        self.header = c.read(4)
        if struct.unpack("<HH", self.header) != (16, 46):
            raise ValueError("Unsupported ABC version")
        self.tables = {}

        def table(name, parse, implicit=False, count=None):
            count = c.number() if count is None else count
            values, records = [], []
            for _ in range(max(0, count - int(implicit))):
                start = c.pos
                values.append(parse())
                records.append(data[start:c.pos])
            value = Table(count, records, ([None] + values) if implicit else values, implicit)
            self.tables[name] = value
            return value.values

        table("integers", c.number, True)
        table("unsigned", c.number, True)
        table("doubles", lambda: c.read(8), True)
        table("strings", lambda: c.read(c.number()), True)
        table("namespaces", lambda: (c.byte(), c.number()), True)
        table("namespace_sets", lambda: [c.number() for _ in range(c.number())], True)

        def name():
            kind = c.byte()
            if kind in (7, 13, 9, 14):
                return kind, c.number(), c.number()
            if kind in (15, 16, 27, 28):
                return kind, c.number()
            if kind in (17, 18):
                return (kind,)
            if kind == 29:
                return kind, c.number(), [c.number() for _ in range(c.number())]
            raise ValueError(f"Unsupported ABC name kind {kind}")

        table("names", name, True)

        def method():
            parameters, returns = c.number(), c.number()
            types = [c.number() for _ in range(parameters)]
            label, flags = c.number(), c.byte()
            if flags & 8:
                for _ in range(c.number()):
                    c.number(); c.byte()
            if flags & 128:
                for _ in range(parameters):
                    c.number()
            return parameters, returns, types, label, flags

        table("methods", method)

        def metadata():
            label, size = c.number(), c.number()
            return label, [c.number() for _ in range(size * 2)]

        table("metadata", metadata)

        def instance():
            ident = c.number()
            prefix_start = c.pos
            parent, flags = c.number(), c.byte()
            if flags & 8:
                c.number()
            for _ in range(c.number()):
                c.number()
            prefix = data[prefix_start:c.pos]
            constructor = c.number()
            suffix_start = c.pos
            fields = c.traits()
            return dict(name=ident, parent=parent, constructor=constructor, traits=fields,
                        prefix=prefix, suffix=data[suffix_start:c.pos])

        self.instances = table("instances", instance)
        table("classes", lambda: (c.number(), c.traits()), count=len(self.instances))
        table("scripts", lambda: (c.number(), c.traits()))

        def body():
            ident = c.number()
            limits = [c.number() for _ in range(4)]
            code = c.read(c.number())
            tail = c.pos
            exceptions = [[c.number() for _ in range(5)] for _ in range(c.number())]
            fields = c.traits()
            return dict(method=ident, limits=limits, code=code, tail=data[tail:c.pos],
                        exceptions=exceptions, traits=fields)

        bodies = table("bodies", body)
        self.bodies = {b["method"]: b for b in bodies}
        if c.pos != len(data) or len(self.bodies) != len(bodies):
            raise ValueError("Malformed ABC method-body table")

    def text(self, name_index):
        name = self.tables["names"].values[name_index]
        string_id = name[2] if name[0] in (7, 13) else name[1] if name[0] in (9, 14, 15, 16) else None
        if string_id is None:
            raise ValueError("Expected a named ABC property")
        return self.tables["strings"].values[string_id] or b""

    def qualified_name(self, name_index):
        name = self.tables["names"].values[name_index]
        if name[0] not in (7, 13):
            raise ValueError("Expected an ABC QName")
        namespace = self.tables["namespaces"].values[name[1]]
        prefix = self.tables["strings"].values[namespace[1]] or b""
        return (prefix + b"." if prefix else b"") + self.text(name_index)

    def append(self, extra):
        result = bytearray(self.header)
        for key in self.POOLS + ("methods", "metadata", "instances", "classes", "scripts", "bodies"):
            if key == "classes":  # ABC shares one count for instances/classes.
                result += b"".join(self.tables[key].records + extra.get(key, []))
            else:
                result += self.tables[key].with_extra(extra.get(key, []))
        return bytes(result)


def instructions(code):
    c, result = Cursor(code), []
    while c.pos < len(code):
        opcode = c.byte()
        if opcode in (0xD0, 0x30, 0x1D, 0x47):
            operands = ()
        elif opcode in (0x24, 0x65):
            operands = (c.byte(),)
        elif opcode in (0x49, 0x5D, 0x60, 0x58, 0x68, 0x66):
            operands = (c.number(),)
        elif opcode == 0x4F:
            operands = (c.number(), c.number())
        else:
            raise ValueError(f"Unsupported template instruction {opcode:#x}")
        result.append((opcode, operands))
    return result


def encode_instructions(ops):
    return b"".join(bytes([opcode]) + (bytes(operands) if opcode in (0x24, 0x65) else b"".join(uint(v) for v in operands))
                    for opcode, operands in ops)


def stop_template(abc):
    for index, instance in enumerate(abc.instances):
        try:
            fields = instance["traits"]
            if abc.text(instance["parent"]) != b"MovieClip":
                continue
            if len(fields) != 1 or fields[0][1] != 1 or abc.text(fields[0][0]) != b"frame1":
                continue
            stop = abc.bodies[fields[0][3]]
            ops = instructions(stop["code"])
            if ([x[0] for x in ops] != [0xD0, 0x30, 0x5D, 0x4F, 0x47] or
                abc.text(ops[2][1][0]) != b"stop" or ops[3][1] != (ops[2][1][0], 0)):
                continue
            constructor = abc.bodies[instance["constructor"]]
            init = instructions(constructor["code"])
            if init[:4] != [(0xD0, ()), (0x30, ()), (0xD0, ()), (0x49, (0,))] or init[-1] != (0x47, ()):
                continue
            block = init[4:-1]
            if (len(block) != 5 or block[0][0] not in (0x5D, 0xD0) or
                block[1] != (0x24, (0,)) or block[2] != (0xD0, ()) or
                block[3][0] != 0x66 or abc.text(block[3][1][0]) != b"frame1" or block[4][0] != 0x4F or
                block[4][1][1] != 2 or abc.text(block[4][1][0]) != b"addFrameScript"):
                continue
            if block[0][0] == 0x5D and block[0][1] != (block[4][1][0],):
                continue
            class_init = abc.tables["classes"].values[index][0]
            if instructions(abc.bodies[class_init]["code"]) != [(0xD0, ()), (0x30, ()), (0x47, ())]:
                continue
            registration, script_slot = next((init_id, traits[0][2]) for init_id, traits in abc.tables["scripts"].values
                                if len(traits) == 1 and traits[0][0] == instance["name"] and
                                traits[0][1] == 4 and traits[0][3] == index)
            script = abc.bodies[registration]
            register_ops = instructions(script["code"])
            mapped = [op for op in register_ops if op[0] in (0x5D, 0x58, 0x68)]
            expected = [(0x58, (index,)), (0x68, (instance["name"],))]
            direct_global = register_ops[:3] == [(0xD0, ()), (0x30, ()), (0x65, (0,))]
            if mapped != [(0x5D, (instance["name"],))] + expected and not (direct_global and mapped == expected):
                continue
            if constructor["exceptions"] or constructor["traits"] or script["exceptions"] or script["traits"]:
                continue
            return index, instance, constructor, block, registration, script, script_slot
        except (KeyError, ValueError, StopIteration, IndexError):
            continue
    raise ValueError("No verified compiler-generated stop-only class is available")


def append_button_class(data, class_name=b"RoomCatListButton", frame_count=6):
    if not 1 <= frame_count <= 127:
        raise ValueError("Button frame count is out of range")
    abc = AbcModule(data)
    index, template, constructor, block, registration, script, script_slot = stop_template(abc)
    if class_name in abc.tables["strings"].values:
        raise ValueError("Button class already exists in the source ABC")
    string_id = max(1, abc.tables["strings"].count)
    name_id = max(1, abc.tables["names"].count)
    class_id = len(abc.instances)
    method_id = abc.tables["methods"].count
    namespace = abc.tables["names"].values[template["name"]][1]
    init = instructions(constructor["code"])[:4]
    for frame in range(frame_count):
        copy = list(block)
        copy[1] = (0x24, (frame,))
        init.extend(copy)
    init.append((0x47, ()))
    register = []
    for opcode, operands in instructions(script["code"]):
        if opcode in (0x5D, 0x68):
            operands = (name_id,)
        elif opcode == 0x58:
            operands = (class_id,)
        register.append((opcode, operands))

    def body(ident, model, code):
        return uint(ident) + b"".join(uint(n) for n in model["limits"]) + uint(len(code)) + code + model["tail"]

    extra = {
        "strings": [uint(len(class_name)) + class_name],
        "names": [b"\x07" + uint(namespace) + uint(string_id)],
        "instances": [uint(name_id) + template["prefix"] + uint(method_id) + template["suffix"]],
        "classes": [abc.tables["classes"].records[index]],
        "methods": [abc.tables["methods"].records[template["constructor"]], abc.tables["methods"].records[registration]],
        "scripts": [uint(method_id+1) + b"\x01" + uint(name_id) + b"\x04" + uint(script_slot) + uint(class_id)],
        "bodies": [body(method_id, constructor, encode_instructions(init)),
                   body(method_id+1, script, encode_instructions(register))],
    }
    merged = AbcModule(abc.append(extra))
    for key, table in abc.tables.items():
        if merged.tables[key].records[:len(table.records)] != table.records:
            raise ValueError(f"Existing ABC records changed in {key}")
    return abc.append(extra), merged.qualified_name(name_id).decode("utf-8")
