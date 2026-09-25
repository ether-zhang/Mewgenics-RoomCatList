"""Validate the pinned MewUI API against the installed executable, without running it."""
from pathlib import Path
import importlib.util
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "vendor" / "mew-ui-api"
SPEC = importlib.util.spec_from_file_location("mew_ui_resolver", UPSTREAM / "re_tools" / "update_mew_ui_api_offsets.py")
resolver = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = resolver
SPEC.loader.exec_module(resolver)


def main():
    image = resolver.PeImage(ROOT.parents[1] / "Mewgenics.exe")
    header = (UPSTREAM / "src" / "native" / "mew_ui_api.h").read_text(encoding="utf-8")
    config = resolver.load_signatures(UPSTREAM / "re_tools" / "mew_ui_api_signatures.json")
    resolver.validate_signature_coverage(UPSTREAM / "src" / "native" / "mew_ui_api.h", config)
    values = {}
    guards = {}
    for name, signature in config["symbols"].items():
        resolved = resolver.resolve_symbol(image, name, signature)
        define = re.search(r"^\s*#define\s+" + re.escape(name) + r"\s+(0x[\da-fA-F]+|\d+)", header, re.M)
        if not define or int(define.group(1), 0) != resolved.value:
            raise RuntimeError(f"Pinned API offset mismatch: {name} resolved to {resolved.value:#x}")
        values[name] = resolved.value
        if name.startswith("MEW_RVA_") and signature.get("kind") == "rva":
            offset, section = image.rva_to_file_offset(resolved.value)
            if section.name == ".text":
                guards[resolved.value] = (name, image.data[offset:offset + 16])
    extra = {
        "RCL_RVA_PROCESS_ARGS": (0x9B8BB0, "4C 89 44 24 18 89 54 24 10 48 89 4C 24 08 55"),
        "RCL_RVA_MOUSE_POSITION": (0x986EE0, "48 89 5C 24 08 57 48 83 EC 60 0F 29 74 24 50"),
        "RCL_VIRTUAL_MOUSE_MOTION_READ": (0x74DA7B, "48 8D 54 24 30 48 8B CF 0F 29 7C 24 50 E8 53 94 23 00"),
        "RCL_VIRTUAL_MOUSE_SCENE_READ": (0x74E6D0, "48 8B 49 38 48 8D 54 24 20 E8 02 88 23 00 48 8B D0"),
        "RCL_VIRTUAL_MOUSE_CACHE": (0x986E52, "80 3D E0 A9 A3 00 00 74 12 F2 0F 10 05 1D 50 97 00 F2 0F 10 0D 1D 50 97 00"),
        "RCL_RVA_DRAW_CURSOR": (0xA20710, "40 55 57 48 8D 6C 24 B1 48 81 EC E8 00 00 00"),
        "RCL_RVA_EFFECTIVE_STATS": (0xC1820, "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57"),
        "RCL_RVA_MUTATION_KEYS": (0xCB690, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55"),
        "RCL_LAYOUT_AGE": (0xD3209, "48 8B 81 40 0C 00 00 48 83 F8 FF 75 06 8B 82 80"),
        "RCL_LAYOUT_GENETIC_STATS": (0xE2DE8, "49 8D BD F0 06 00 00 48 89 BD A8 00 00 00"),
        "RCL_RVA_ADD_CAT_TO_LOCATION": (0x2E88D0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F9 48 8B F2 48 8B 49 70 4C 8B C2 8B 47 6C"),
        "RCL_RVA_ROOM_BOUNDS": (0x2EA460, "48 8B 41 38 66 0F 6E 81 F0 00 00 00 F3 0F E6 C0"),
        "RCL_RVA_ROOM_EFFECTS": (0x2EABE0, "40 53 48 83 EC 20 80 B9 58 01 00 00 00 48 8B D9"),
        "RCL_LAYOUT_CROWDING": (0x2EA8E1, "8B 57 6C 33 C0 83 EA 04 48 89 5C 24 40 85 D2 48"),
        "RCL_LAYOUT_CAT_ROOM_EFFECTS": (0x2EAEC2, "48 8B 17 48 81 C2 A0 00 00 00 49 8D 8D 40 01 00 00"),
        "RCL_COMFORT_EFFECT_SCALE": (0x1B518D, "F2 0F 59 35 A3 A7 F8 00 E9 07 02 00 00"),
        "RCL_RVA_CAN_ENTER_BOX": (0xAEC00, "40 53 48 83 EC 20 48 8B DA 48 85 D2 74 49"),
        "RCL_LAYOUT_BOX_SLOTS": (0xAF960, "48 8B 81 00 01 00 00 45 33 C9 48 39 10 75 03 4C"),
        "RCL_RVA_CAN_GIVE_CAT": (0x276600, "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41"),
        "RCL_RVA_OPEN_CAT": (0xEC7B0, "40 53 48 83 EC 40 48 8B D9 48 85 D2 0F 84 F3 00"),
        "RCL_LAYOUT_OPEN_CAT": (0xEC7C2, "48 89 74 24 50 41 B0 01 48 89 7C 24 60 E8 CC F3"),
        "RCL_RVA_PIPE_ENTER": (0x208C40, "48 8B C4 48 89 58 18 48 89 50 10 55 56 57 41 54"),
        "RCL_LAYOUT_PIPE_OCCUPANCY_EDGE": (0x208324, "44 39 A6 F0 00 00 00 0F 85 D8 00 00 00 48 85 DB"),
        "RCL_LAYOUT_NPC_UNLOCK": (0x276646, "80 7C 0A 48 00 0F 85 9A 01 00 00 80 7C 0A 51 00"),
        "RCL_LAYOUT_SEX": (0xAA80E, "41 89 74 24 58 48 8D 4C 24 50 49 83 FE 0F 48 0F"),
        "RCL_LAYOUT_ORIENTATION": (0xE3F23, "66 45 0F 2F 85 C0 0B 00 00 76 11 48 8D 15 23 75"),
        "RCL_LAYOUT_MARKER": (0xEDAB8, "48 8D 48 38 48 3B CA 74 13 48 83 7A 18 0F 4C 8B"),
        "RCL_RVA_ASSIGN_MARKER": (0x520D0, "48 89 5C 24 10 48 89 6C 24 18 56 57 41 57 48 83"),
        "RCL_RVA_REFRESH_CAT_PANEL": (0xEBBA0, "48 89 5C 24 10 55 56 57 41 54 41 55 41 56 41 57"),
        "RCL_LAYOUT_MARKER_REFRESH": (0xEDAD4, "C6 43 70 01 48 8B 53 78 48 85 D2 74 0D 48 8B 83"),
        "RCL_LAYOUT_CLASS_ICON": (0xE2FD2, "49 8D 95 10 0C 00 00 48 8D 4D 80 E8 7E F4 F6 FF"),
        "RCL_NPC_PIPE_STEVEN_DISABLED": (0x27A451, "8B 05 E5 83 C8 00 89 44 24 40 0F B7 05 DE 83 C8"),
        "RCL_RVA_CLICK_BEANIES": (0x27BC20, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55"),
        "RCL_RVA_CLICK_BUTCH": (0x27BEC0, "48 89 5C 24 10 48 89 7C 24 18 55 48 8D 6C 24 A9"),
        "RCL_RVA_CLICK_TINK": (0x27B9A0, "48 89 5C 24 10 48 89 7C 24 18 55 48 8D 6C 24 A9"),
        "RCL_RVA_CLICK_FRANK": (0x27C790, "48 89 5C 24 10 48 89 7C 24 18 55 48 8D 6C 24 A9"),
        "RCL_RVA_CLICK_JACK": (0x27C150, "48 89 5C 24 10 48 89 7C 24 18 55 48 8D 6C 24 A9"),
        "RCL_RVA_CLICK_TRACY": (0x27C320, "48 89 5C 24 10 48 89 7C 24 18 55 48 8D 6C 24 A9"),
        "RCL_RVA_CLICK_ORGAN": (0x27C500, "48 89 5C 24 10 48 89 7C 24 18 55 48 8D 6C 24 A9"),
        "RCL_RVA_GET_PARENT_CAT": (0xD7220, "48 89 5C 24 08 48 89 74 24 20 48 89 54 24 10 57"),
        "RCL_LAYOUT_PEDIGREE": (0xD74CF, "48 8B F9 48 8B DA 48 83 C1 38 E8 32 BE 69 00 48"),
        "RCL_LAYOUT_INBREEDING": (0xE48C1, "66 45 0F 2F 85 50 0C 00 00 73 11 48 8D 15 B5 6B"),
        "RCL_LAYOUT_MAP_REQUEST_CONSUMED": (0x27B1B3, "48 8B 85 C0 01 00 00 C6 40 40 00 48 8B C8 48 81"),
        "RCL_LAYOUT_MAP_DONATION_MODE": (0x27BC3C, "80 79 41 00 48 8B F9 0F 84 55 01 00 00 48 8B 49"),
        "RCL_RVA_DISCARD_MENU": (0x27E1A0, "48 89 5C 24 10 55 48 8D 6C 24 A9 48 81 EC B0 00"),
        "RCL_RVA_GAMEPAD_BUTTON": (0xBBEF00, "40 56 57 41 57 48 83 EC 20 44 8B FA 48 8B F1 40"),
        "RCL_RVA_GAMEPAD_AXIS": (0xBBEAE0, "40 55 41 54 41 56 48 83 EC 20 44 8B E2 4C 8B F1"),
    }
    for name, (rva, expected) in extra.items():
        offset, _ = image.rva_to_file_offset(rva)
        prefix = bytes.fromhex(expected)
        if image.data[offset:offset + len(prefix)] != prefix:
            raise RuntimeError(f"Native entry-point signature mismatch: {name}")
        guards[rva] = (name, image.data[offset:offset + 16])
    lines = ["// Generated only after upstream signature resolution matches the pinned header.",
             "#pragma once", "#include <cstdint>",
             "struct RoomCatNativeSignature { std::uintptr_t rva; unsigned char bytes[16]; const char* name; };",
             "inline constexpr RoomCatNativeSignature kRoomCatNativeSignatures[] = {"]
    for rva, (name, data) in sorted(guards.items()):
        lines.append("    {0x%X, {%s}, \"%s\"}," % (rva, ", ".join(f"0x{x:02X}" for x in data), name))
    lines += ["};", ""]
    destination = ROOT / "src" / "native_signatures.generated.hpp"
    text = "\n".join(lines)
    if not destination.exists() or destination.read_text(encoding="utf-8") != text:
        destination.write_text(text, encoding="utf-8")
    print(f"Native API verified: {len(values)} upstream symbols; {len(guards)} code guards. Game was not launched.")


if __name__ == "__main__":
    main()
