// Exercise the production placement/rollback code on synthetic memory only.
// No game process or real cat is accessed by this test.
#include "../src/native_moves.cpp"
#include <cassert>
#include <iostream>

namespace {
std::array<unsigned char, 0x180> cat{}, area{}, transform{}, room_transform{};
int calls = 0, behavior = 0;
template<class T> void Put(std::array<unsigned char, 0x180>& block, int offset, T value) {
    std::memcpy(block.data() + offset, &value, sizeof(value));
}
std::array<double, 3> Position() {
    std::array<double, 3> value{};
    std::memcpy(value.data(), transform.data() + 0x80, sizeof(value));
    return value;
}
void __fastcall Add(void* destination, void* actor) {
    ++calls;
    assert(destination == area.data() && actor == cat.data());
    if (behavior == 2) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    if (behavior == 1) return;
    if (behavior == 3) {
        assert(Position() == (std::array<double, 3>{3, 4, 5})); // Box handles its own positioning.
        Put(transform, 0x80, std::array<double, 3>{50, 51, 52});
    } else assert(Position() == (std::array<double, 3>{20, 31.5, 7}));
    Put(cat, 0xE8, reinterpret_cast<std::uintptr_t>(destination));
}
}

int main() {
    using namespace roomcats;
    Put(cat, 0x40, reinterpret_cast<std::uintptr_t>(transform.data()));
    Put(area, 0x38, reinterpret_cast<std::uintptr_t>(room_transform.data()));
    Put(room_transform, 0x90, 7.0);
    g_add_cat = Add;
    RoomBounds bounds{10, 30, 30, 40};
    for (int mode = 0; mode < 4; ++mode) {
        behavior = mode;
        Put(cat, 0xE8, std::uintptr_t(123));
        Put(transform, 0x80, std::array<double, 3>{3, 4, 5});
        const bool moved = CommitMove(reinterpret_cast<std::uintptr_t>(cat.data()),
            reinterpret_cast<std::uintptr_t>(area.data()), mode != 3, &bounds);
        assert(moved == (mode == 0 || mode == 3));
        if (!moved) assert(Position() == (std::array<double, 3>{3, 4, 5}));
    }
    assert(calls == 4);
    std::cout << "Native placement passed: coordinates before room entry, native box positioning, rejected/exception rollback and no retries.\n";
}
