#include "../src/game_reader.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>

namespace {
template<std::size_t N> using Block = std::array<unsigned char,N>;
template<class T> std::uintptr_t Address(T& value) { return reinterpret_cast<std::uintptr_t>(value.data()); }
template<class T> void Put(std::uintptr_t address, unsigned offset, const T& value) {
    std::memcpy(reinterpret_cast<void*>(address+offset), &value, sizeof(value));
}
struct Record { std::uint64_t child; std::int64_t a,b; double coi; };
struct Header { std::uintptr_t controls, slots; std::uint64_t size, capacity; };
static_assert(sizeof(Record) == 32);
struct Fixture {
    Block<0x160> manager{};
    std::array<unsigned char,48> controls{};
    std::array<Record,31> slots{};
    Block<0xC58> parent_a{},parent_b{};
    Block<0x20> node_a{},node_b{},sentinel{};
    std::array<std::uintptr_t,2> buckets{};
    Header header;
    Fixture() : header{Address(controls),Address(slots),2,31} {
        controls.fill(0x80);
        // Golden phmap hashes: child 42 -> h2 94, first group 12;
        // child 43 -> h2 72, first group 30 (wraps to the clone of slot 1).
        slots[12] = {42,1001,1002,0.25}; controls[12] = 94;
        slots[1] = {43,2001,-1,0}; controls[1] = 72;
        CloneControls();
        Put(Address(manager),0x38,header);
        Name(parent_a,L"父名",1001); Name(parent_b,L"母名",1002);
        Put(Address(node_a),0x10,std::uint64_t(1001)); Put(Address(node_a),0x18,Address(parent_a));
        Put(Address(node_b),0x10,std::uint64_t(1002)); Put(Address(node_b),0x18,Address(parent_b));
        Put(Address(node_a),8,Address(sentinel)); Put(Address(node_b),8,Address(node_a));
        buckets = {Address(node_a),Address(node_a)};
        Put(Address(manager),0xF0,Address(sentinel)); Put(Address(manager),0x100,Address(buckets));
    }
    void CloneControls() {
        controls[31] = 0xFF;
        for (int i=0;i<15;++i) controls[32+i] = controls[i];
    }
    static void Name(Block<0xC58>& data, const wchar_t* name, std::uint64_t id) {
        const auto length = std::wcslen(name);
        assert(length <= 7);
        std::memset(data.data()+0x18,0,32);
        std::memcpy(data.data()+0x18,name,length*2);
        Put(Address(data),0x28,std::uint64_t(length)); Put(Address(data),0x30,std::uint64_t(7));
        Put(Address(data),0xC48,id);
    }
};
Fixture* fixture = nullptr;
int loads = 0;
std::uintptr_t Loader(std::uintptr_t manager, std::uint64_t id) {
    assert(manager == Address(fixture->manager));
    ++loads;
    if (id == 1002) {
        fixture->buckets[1] = Address(fixture->node_b); // Like the engine, populate its cat cache.
        return Address(fixture->parent_b);
    }
    if (id == 3001) return Address(fixture->parent_b); // Wrong returned identity must be rejected.
    return 0;
}
}

void TestFamilyReader() {
    using namespace roomcats;
    Fixture f; fixture = &f;
    ConfigureParentCatLoader(Loader);
    const auto pedigree = Address(f.manager)+0x38;
    std::array<std::uint64_t,2> parents{};
    assert(ReadParentIds(pedigree,42,parents) && (parents == std::array<std::uint64_t,2>{1001,1002}));
    assert(ReadParentIds(pedigree,43,parents) && (parents == std::array<std::uint64_t,2>{2001,0}));
    assert(!ReadParentIds(pedigree,99,parents) && (parents == std::array<std::uint64_t,2>{}));
    const auto before = f.slots;
    auto family = ReadCatFamily(Address(f.manager),42,10);
    assert(family.valid && family.names[0] == "父名" && family.names[1] == "母名" && loads == 1);
    for (int i=0;i<100;++i) assert(ReadCatFamily(Address(f.manager),42,10).names == family.names);
    assert(loads == 1 && std::memcmp(before.data(),f.slots.data(),sizeof(before)) == 0);
    Fixture::Name(f.parent_a,L"已改名",1001);
    assert(ReadCatFamily(Address(f.manager),42,10).names[0] == "已改名");
    assert(ReadCatFamily(Address(f.manager),43,10).names[0].empty() && loads == 2);
    for (int i=0;i<100;++i) ReadCatFamily(Address(f.manager),43,10);
    assert(loads == 2);
    ReadCatFamily(Address(f.manager),43,11);
    assert(loads == 3); // A failed historical query is not retried every frame.
    f.slots[1].a = 3001;
    assert(ReadCatFamily(Address(f.manager),43,11).names[0].empty() && loads == 4);
    f.slots[1].a = -1;
    family = ReadCatFamily(Address(f.manager),43,11);
    assert(family.valid && !family.ids[0] && !family.ids[1] && loads == 4);
    f.slots[1].a = 43;
    assert(!ReadParentIds(pedigree,43,parents)); // Self-parent cannot be a valid pedigree.
    f.slots[1].a = -2;
    assert(!ReadParentIds(pedigree,43,parents));
    f.slots[1].a = 1001; f.slots[1].coi = std::numeric_limits<double>::quiet_NaN();
    assert(!ReadParentIds(pedigree,43,parents));
    // An occupied/deleted first probe group must advance by 16, without
    // mistaking tombstones or the sentinel for an empty slot.
    f.controls.fill(0xFE); f.slots[29] = {42,1001,1002,0}; f.controls[29] = 94; f.CloneControls();
    assert(ReadParentIds(pedigree,42,parents) && parents[1] == 1002);
    assert(!ReadParentIds(pedigree,99,parents)); // Bounded termination even with no empty controls.
    auto bad = f.header; bad.capacity = 30;
    Put(Address(f.manager),0x38,bad);
    assert(!ReadParentIds(pedigree,42,parents));
    bad = f.header; bad.size = 32; Put(Address(f.manager),0x38,bad);
    assert(!ReadParentIds(pedigree,42,parents));
    assert(!ReadParentIds(1,42,parents));
    ConfigureParentCatLoader(nullptr);
}
