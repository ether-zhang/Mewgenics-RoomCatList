#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../src/game_reader.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>

struct TestString { unsigned char storage[16]{}; std::uint64_t size=0, capacity=0; };
void TestRoomScopes();
void TestNativeArgs();
void TestFamilyReader();
void TestComfortReader();

int main() {
    roomcats::SetLanguage(roomcats::Language::Chinese);
    std::string result;
    TestString small;
    small.size=2; small.capacity=7;
    std::memcpy(small.storage, L"清歌", 4);
    assert(roomcats::ReadWideString(reinterpret_cast<std::uintptr_t>(&small), result));
    assert(result == "清歌");

    const wchar_t long_name[] = L"很长的中文猫咪名字##仍然完整";
    TestString heap;
    const wchar_t* pointer = long_name;
    std::memcpy(heap.storage, &pointer, sizeof(pointer));
    heap.size = wcslen(long_name); heap.capacity=64;
    assert(roomcats::ReadWideString(reinterpret_cast<std::uintptr_t>(&heap), result));
    assert(result == "很长的中文猫咪名字##仍然完整");

    TestString empty; empty.capacity=7;
    assert(roomcats::ReadWideString(reinterpret_cast<std::uintptr_t>(&empty), result) && result.empty());
    small.size = 300;
    assert(!roomcats::ReadWideString(reinterpret_cast<std::uintptr_t>(&small), result));
    small.size = 8;
    assert(!roomcats::ReadWideString(reinterpret_cast<std::uintptr_t>(&small), result));
    pointer = reinterpret_cast<const wchar_t*>(1);
    std::memcpy(heap.storage, &pointer, sizeof(pointer));
    assert(!roomcats::ReadWideString(reinterpret_cast<std::uintptr_t>(&heap), result));
    assert(!roomcats::ReadWideString(0, result));

    TestString surrogate;
    surrogate.capacity = 7; surrogate.size = 1;
    const wchar_t lone_surrogate = 0xD800;
    std::memcpy(surrogate.storage, &lone_surrogate, 2);
    assert(!roomcats::ReadWideString(reinterpret_cast<std::uintptr_t>(&surrogate), result));

    TestString narrow;
    narrow.capacity=15; narrow.size=13;
    std::memcpy(narrow.storage, "Floor2_Small", 13);
    // The deliberately embedded NUL is rejected instead of changing a room ID.
    assert(!roomcats::ReadNarrowString(reinterpret_cast<std::uintptr_t>(&narrow), result));
    narrow.size=12;
    assert(roomcats::ReadNarrowString(reinterpret_cast<std::uintptr_t>(&narrow), result));
    assert(result == "Floor2_Small");
    assert(roomcats::RoomLabel(result) == "二楼左");
    assert(roomcats::RoomLabel("Floor1_Large") == "一楼左");
    assert(roomcats::RoomLabel("Floor1_Small") == "一楼右");
    assert(roomcats::RoomLabel("Floor2_Large") == "二楼右");
    assert(roomcats::RoomLabel("CustomRoom") == "CustomRoom");
    roomcats::SetLanguage(roomcats::Language::English);
    assert(roomcats::RoomLabel("Floor2_Small") == "2F left");
    assert(roomcats::LocalizedLocationLabel("room:Attic","阁楼") == "Attic");
    assert(roomcats::LocalizedLocationLabel("box:1","箱子 2") == "Box 2");
    roomcats::SetLanguage(roomcats::Language::Chinese);
    assert(roomcats::LocalizedLocationLabel("room:Attic","Attic") == "阁楼");
    assert(roomcats::LocalizedLocationLabel("box:1","Box 2") == "箱子 2");
    assert(roomcats::LocalizedLocationLabel("","Deceased") == "已故");
    assert(roomcats::HashCatId(0) == 0xA8C7F832281A39C5ULL);
    assert(roomcats::HashCatId(1) != roomcats::HashCatId(256));

    std::string reason;
    assert(!roomcats::ValidateGameBuild(0, reason) && !reason.empty());
    auto snapshot = roomcats::ReadFocusedRoom(0, true);
    assert(!snapshot.in_house && snapshot.cats.empty());
    TestRoomScopes();
    TestNativeArgs();
    TestFamilyReader();
    TestComfortReader();
    std::cout << "Family reader passed: parent IDs, wrapped probes, native name loading, changed names, unknown parents, cache reuse and invalid data.\n";
    std::cout << "Reader checks passed: strings, invalid memory, build rejection, whole-house scope, focused rooms, empty rooms, duplicate names, scope changes and scene teardown.\n";
}
