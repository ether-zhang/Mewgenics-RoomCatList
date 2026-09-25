"""Regression checks for the engine's single-DoABC native HUD resource."""
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build_sidebar_swf as swf
from abc_patch import AbcModule, append_button_class, instructions, stop_template
from build_ui_theme import NativeUi, shapes, edge_mask


def abc_payload(body):
    return body[body.index(0, 4) + 1:]


def tables(data):
    return list(swf.read_tags(data, swf.tag_start(data)))


class SidebarTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = swf.read_base_swf()
        cls.generated = swf.build(cls.base)
        cls.base_tags = tables(cls.base)
        cls.result_tags = tables(cls.generated)
        cls.original_abc_body = next(b for k, b, _ in cls.base_tags if k == 82)
        cls.result_abc_body = next(b for k, b, _ in cls.result_tags if k == 82)
        cls.original_abc = AbcModule(abc_payload(cls.original_abc_body))
        cls.result_abc = AbcModule(abc_payload(cls.result_abc_body))

    def test_exactly_one_doabc_in_delivered_resource(self):
        self.assertEqual(sum(k == 82 for k, _, _ in self.result_tags), 1)
        self.assertEqual(self.generated, (ROOT / "build" / "swfs" / "house.swf").read_bytes())
        delivered = (ROOT / "build" / "swfs" / "house.swf").read_bytes()
        self.assertEqual(delivered[:3],b'FWS')
        self.assertEqual(sum(k == 82 for k, _, _ in tables(delivered)), 1)
        self.assertEqual(struct.unpack_from("<I", delivered, 4)[0], len(delivered))
        prefix_end = self.original_abc_body.index(0, 4) + 1
        self.assertEqual(self.result_abc_body[:prefix_end], self.original_abc_body[:prefix_end])

    def test_existing_abc_indexes_and_bytecode_preserved(self):
        delta = {"strings": 1, "names": 1, "methods": 2, "instances": 1,
                 "classes": 1, "scripts": 1, "bodies": 2}
        for key, original in self.original_abc.tables.items():
            result = self.result_abc.tables[key]
            self.assertEqual(result.records[:len(original.records)], original.records, key)
            self.assertEqual(result.count, original.count + delta.get(key, 0), key)
        for ident, body in self.original_abc.bodies.items():
            self.assertEqual(self.result_abc.bodies[ident], body)

    def test_button_uses_compiler_scope_and_six_stop_frames(self):
        _, template, constructor, _, _, script, slot = stop_template(self.original_abc)
        added = self.result_abc.instances[-1]
        self.assertEqual(self.result_abc.text(added["name"]), b"RoomCatListButton")
        self.assertEqual(added["parent"], template["parent"])
        self.assertEqual(added["traits"], template["traits"])
        new_constructor = self.result_abc.bodies[added["constructor"]]
        self.assertEqual(new_constructor["limits"], constructor["limits"])
        ops = instructions(new_constructor["code"])
        self.assertEqual([operands[0] for opcode, operands in ops if opcode == 0x24], list(range(6)))
        calls = [operands for opcode, operands in ops if opcode == 0x4F]
        self.assertEqual(len(calls), 6)
        self.assertTrue(all(self.result_abc.text(name) == b"addFrameScript" and argc == 2 for name, argc in calls))
        script_id, traits = self.result_abc.tables["scripts"].values[-1]
        self.assertEqual(traits, [(added["name"], 4, slot, len(self.original_abc.instances))])
        registration = self.result_abc.bodies[script_id]
        self.assertEqual(registration["limits"], script["limits"])
        self.assertIn((0x58, (len(self.original_abc.instances),)), instructions(registration["code"]))
        self.assertIn((0x68, (added["name"],)), instructions(registration["code"]))

    def test_native_states_and_symbol_binding(self):
        symbols = {name: ident for k, b, _ in self.result_tags if k == 76 for ident, name in swf.symbol_entries(b)}
        sprites = {struct.unpack_from("<H", b)[0]: b for k, b, _ in self.result_tags if k == 39}
        exported = self.result_abc.qualified_name(self.result_abc.instances[-1]["name"]).decode()
        button = sprites[symbols[exported]]
        self.assertEqual(struct.unpack_from("<H", button, 2)[0], 6)
        button_tags = list(swf.read_tags(button, 4))
        self.assertEqual([body.rstrip(b"\0") for kind, body, _ in button_tags if kind == 43],
                         [b"up", b"over", b"down", b"selected", b"disabled", b"enable"])
        self.assertEqual(sum(kind == 1 for kind, _, _ in button_tags), 6)
        self.assertEqual(sum(kind == 28 for kind, _, _ in button_tags), 10)
        self.assertIn(b"rcl_button\0", sprites[symbols["HouseStatusUI"]])

    def test_original_graphics_and_hud_preserved(self):
        old_defs = {struct.unpack_from("<H", b)[0]: (k, b) for k, b, _ in self.base_tags if k in swf.DEFINITION_TAGS}
        new_defs = {struct.unpack_from("<H", b)[0]: (k, b) for k, b, _ in self.result_tags if k in swf.DEFINITION_TAGS}
        old_symbols = [item for k, b, _ in self.base_tags if k == 76 for item in swf.symbol_entries(b)]
        hud_id = next(i for i, name in old_symbols if name == "HouseStatusUI")
        for ident, definition in old_defs.items():
            if ident != hud_id:
                self.assertEqual(new_defs[ident], definition)
        hud = new_defs[hud_id][1]
        restored = hud[:4] + b"".join(raw for k, b, raw in swf.read_tags(hud, 4)
                                     if not (k == 26 and b"rcl_button\0" in b))
        self.assertEqual(restored, old_defs[hud_id][1])
        self.assertEqual(len(new_defs) - len(old_defs), 2)
        self.assertTrue(all(kind in (32, 39) for i, (kind, _) in new_defs.items() if i not in old_defs))
        new_symbols = [item for k, b, _ in self.result_tags if k == 76 for item in swf.symbol_entries(b)]
        self.assertEqual(new_symbols[:-1], old_symbols)

    def test_native_paper_and_hover_pressed_effects_are_reused(self):
        _,_,reference=swf.sidebar_template(self.base_tags)
        result=NativeUi.from_tags(self.result_tags)
        exported=self.result_abc.qualified_name(self.result_abc.instances[-1]['name']).decode()
        ident=result.symbols[exported]
        for state,original in (('up','up'),('over','over'),('down','down'),('selected','down'),('disabled','disabled'),('enable','up')):
            frame=result.frame(ident,label=state)
            self.assertEqual(len(frame),2)
            self.assertEqual(frame[0],reference[original][0]) # Unmodified native paper/transform/tint.
            self.assertEqual(frame[1][1:],reference[original][1][1:])
            self.assertNotEqual(frame[1][0],reference[original][1][0])
        up,over,down=(result.frame(ident,label=state) for state in ('up','over','down'))
        self.assertGreater(over[1][1][0],up[1][1][0])
        self.assertLess(down[1][1][0],up[1][1][0])
        self.assertGreater(over[0][2][1][0],down[0][2][1][0])

    def test_cat_head_and_list_have_separate_visible_ink(self):
        result=NativeUi.from_tags(self.result_tags)
        exported=self.result_abc.qualified_name(self.result_abc.instances[-1]['name']).decode()
        icon=result.frame(result.symbols[exported],label='up')[1][0]
        kind,body=result.defs[icon]
        groups=shapes(kind,body)
        self.assertEqual(len(groups),1) # Only ink; paper is a separate display layer.
        edges=[((a[0]/20,a[1]/20),(b[0]/20,b[1]/20)) for a,b in groups[0][1]]
        mask=edge_mask(edges,(152,144))
        for point in ((49,60),(15,35),(104,42),(130,42),(130,74),(130,106)):
            self.assertEqual(mask.getpixel(point),255,point)
        for point in ((31,76),(66,76),(49,98),(130,60),(130,90)):
            self.assertEqual(mask.getpixel(point),0,point)

    def test_multiple_doabc_source_is_rejected(self):
        invalid = bytearray(self.base[:swf.tag_start(self.base)])
        for kind, body, raw in self.base_tags:
            if kind == 82:
                invalid += swf.tag(82, body)
            invalid += raw
        struct.pack_into("<I", invalid, 4, len(invalid))
        with self.assertRaisesRegex(ValueError, "exactly one DoABC"):
            swf.build(bytes(invalid))

    def test_duplicate_class_and_truncation_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "already exists"):
            append_button_class(abc_payload(self.result_abc_body))
        with self.assertRaises(ValueError):
            AbcModule(abc_payload(self.original_abc_body)[:-1])


if __name__ == "__main__":
    unittest.main()
