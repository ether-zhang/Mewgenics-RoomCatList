#include "../src/game_reader.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <vector>

namespace {
int detail_reads = 0;
bool ScopeDetails(std::uintptr_t, roomcats::NativeCatDetails& result) {
    ++detail_reads; result.real.fill(7); return true;
}
template<std::size_t N> using Block = std::array<unsigned char, N>;
template<class Buffer, class Value> void Put(Buffer& buffer, std::size_t offset, const Value& value) {
    assert(offset + sizeof(value) <= buffer.size());
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
}
template<class Buffer> std::uintptr_t Address(Buffer& buffer) {
    return reinterpret_cast<std::uintptr_t>(buffer.data());
}
template<class Buffer> void Narrow(Buffer& buffer, std::size_t offset, const char* text) {
    const auto length = std::strlen(text);
    assert(length <= 15 && offset + 32 <= buffer.size());
    std::memcpy(buffer.data() + offset, text, length);
    Put(buffer, offset + 16, std::uint64_t(length));
    Put(buffer, offset + 24, std::uint64_t(15));
}
template<class Buffer> void Wide(Buffer& buffer, std::size_t offset, const wchar_t* text) {
    const auto length = std::wcslen(text);
    assert(length <= 7 && offset + 32 <= buffer.size());
    std::memcpy(buffer.data() + offset, text, length * 2);
    Put(buffer, offset + 16, std::uint64_t(length));
    Put(buffer, offset + 24, std::uint64_t(7));
}

// A small synthetic House scene exercises the production reader end to end.
// It never attaches to a process or accesses the player's game/save data.
struct HouseFixture {
    std::vector<unsigned char> image = std::vector<unsigned char>(0x1400000);
    Block<0x600> director{}, scene{};
    Block<0x30> root{};
    Block<0x130> manager{};
    Block<0x100> house{};
    Block<0xD0> hud{};
    Block<0x80> room_a{}, room_b{}, empty_room{};
    Block<0x120> box{};
    Block<0xF0> cat_a{}, cat_b{}, cat_without_room{};
    Block<0xC58> data_a{}, data_b{}, data_c{};
    Block<0x20> node_a{}, node_b{}, node_c{}, sentinel{};
    std::array<std::uintptr_t, 2> buckets{};
    std::array<std::uintptr_t, 1> scenes{};
    std::array<std::uintptr_t, 8> components{};
    Block<16> component_list{};

    std::uintptr_t Type(std::uint32_t rva, const char* name) {
        const std::uint32_t locator = rva + 0x80;
        const std::uint32_t vtable = rva + 0xB0;
        std::memcpy(image.data() + rva + 16, name, std::strlen(name) + 1);
        Put(image, locator, std::uint32_t(1));
        Put(image, locator + 12, rva);
        Put(image, locator + 20, locator);
        Put(image, vtable - 8, Address(image) + locator);
        return Address(image) + vtable;
    }

    HouseFixture() {
        Put(image, 0x13DAC30, Address(director));
        Put(director, 0x28, Address(root));
        Put(director, 0x598, Address(manager));
        Put(director,0x580,int(40));
        scenes[0] = Address(scene);
        Put(root, 0, Address(scenes));
        Put(root, 8, Address(scenes) + sizeof(scenes));
        Narrow(scene, 0x4B8, "House");

        const auto house_type = Type(0x10000, ".?AVHouse@glaiel@@");
        const auto room_type = Type(0x11000, ".?AVFurnitureGrid@glaiel@@");
        const auto cat_type = Type(0x12000, ".?AVHouseCat@glaiel@@");
        Put(box, 0, Type(0x13000, ".?AVButchBox@glaiel@@"));
        Put(box, 0x20, Address(scene));
        Put(house, 0, house_type);
        Put(house, 0x48, Address(hud));
        Put(hud, 0x20, Address(scene));
        Narrow(hud, 0xA8, "HouseStatusUI");
        for (auto* room : {&room_a, &room_b, &empty_room}) {
            Put(*room, 0, room_type);
            Put(*room, 0x20, Address(scene));
        }
        Narrow(room_a, 0x40, "Floor1_Small");
        Narrow(room_b, 0x40, "Floor2_Small");
        Narrow(empty_room, 0x40, "Attic");
        for (auto* cat : {&cat_a, &cat_b, &cat_without_room}) {
            Put(*cat, 0, cat_type); Put(*cat,0x20,Address(scene)); Put(*cat,0x88,std::uint8_t(1));
        }
        Put(cat_a, 0x80, std::uint64_t(11));
        Put(cat_b, 0x80, std::uint64_t(22));
        Put(cat_without_room, 0x80, std::uint64_t(33));
        Put(cat_a, 0xE8, Address(room_a));
        Put(cat_b, 0xE8, Address(room_b));
        Wide(data_a, 0x18, L"阿花");
        Wide(data_b, 0x18, L"阿花");
        Wide(data_c, 0x18, L"小白");
        Put(data_a,0xC48,std::uint64_t(11)); Put(data_b,0xC48,std::uint64_t(22)); Put(data_c,0xC48,std::uint64_t(33));
        for (auto* data : {&data_a,&data_b,&data_c}) {
            Put(*data,0xC40,std::int64_t(-1));
            roomcats::Stats genes; genes.fill(5); Put(*data,0x6F0,genes);
            Narrow(*data,0xC10,"Mage");
        }

        Put(node_a, 8, Address(sentinel));
        Put(node_a, 0x10, std::uint64_t(11));
        Put(node_a, 0x18, Address(data_a));
        Put(node_b, 8, Address(node_a));
        Put(node_b, 0x10, std::uint64_t(22));
        Put(node_b, 0x18, Address(data_b));
        Put(node_c, 8, Address(node_b));
        Put(node_c, 0x10, std::uint64_t(33));
        Put(node_c, 0x18, Address(data_c));
        buckets = {Address(node_a), Address(node_c)};
        Put(manager, 0xF0, Address(sentinel));
        Put(manager, 0x100, Address(buckets));
        Put(manager, 0x118, std::uint64_t(0));
        components = {Address(house), Address(room_a), Address(room_b), Address(empty_room),
                      Address(cat_a), Address(cat_b), Address(cat_without_room), Address(box)};
        Put(component_list, 0, std::uint32_t(components.size()));
        Put(component_list, 4, std::uint32_t(components.size()));
        Put(component_list, 8, Address(components));
        Put(scene, 0x18, Address(component_list));
    }
    roomcats::RoomSnapshot Read(bool names = true) {
        return roomcats::ReadFocusedRoom(Address(image), names);
    }
};
}

void TestRoomScopes() {
    roomcats::ConfigureCatDetailsReader(ScopeDetails);
    HouseFixture f;
    assert(roomcats::FindHouseHudRenderer(Address(f.image), Address(f.scene)) == Address(f.hud));
    auto all = f.Read();
    assert(all.in_house && all.valid && !all.room && all.cats.size() == 3);
    assert(all.cat_database==Address(f.manager) && f.Read(false).cat_database==all.cat_database);
    assert(all.cats[0].name == "阿花" && all.cats[1].name == "阿花");
    assert(all.cats[0].id != all.cats[1].id);
    assert(all.locations.size() == 4 && all.locations.back().kind == roomcats::LocationKind::Box);
    assert(all.cats[0].location_key == "room:Floor1_Small" && all.cats[0].component == Address(f.cat_a));
    assert(all.cats[2].location_label == "未分配");
    assert(roomcats::RoomLabel(all.room_id) == "全屋猫咪");

    Put(f.house, 0x88, Address(f.room_a));
    auto focused = f.Read();
    assert(focused.valid && focused.room_id == "Floor1_Small" && focused.cats.size() == 1);
    assert(focused.cats[0].id == 11);
    assert(roomcats::ReadFocusedRoom(Address(f.image), true, true).cats.size() == 3);
    const auto parents = roomcats::ReadParentCats(Address(f.image),{22,33});
    assert(parents.valid && parents.cats.size()==2 && !parents.room);
    assert(parents.cats[0].id==22 && parents.cats[0].component==Address(f.cat_b) && !parents.cats[0].record_only);
    assert(parents.cats[0].location==Address(f.room_b) && parents.locations.size()==4);
    Put(f.cat_without_room,0x88,std::uint8_t(0));
    Put(f.data_c,0xC40,std::int64_t(30));
    const auto historical = roomcats::ReadParentCats(Address(f.image),{33,999},{"","Known parent"});
    assert(historical.valid && historical.cats.size()==2 && historical.cats[0].record_only);
    assert(historical.cats[0].name=="小白" && historical.cats[0].location_label=="已故" && !historical.cats[0].component);
    assert(historical.cats[0].details.valid && historical.cats[0].details.age==30 && historical.cats[0].details.collar=="Mage");
    assert(historical.cats[0].details.real[0]==7 && historical.cats[0].details.genetic[0]==5);
    assert(historical.cats[1].record_only && historical.cats[1].name=="Known parent" && !historical.cats[1].details.valid);
    Put(f.node_c,0x18,std::uintptr_t(0)); // Native trash removes data before the inactive actor disappears.
    const auto after_discard=roomcats::ReadFocusedRoom(Address(f.image),true,true);
    assert(after_discard.valid && after_discard.cats.size()==2);
    Put(f.node_c,0x18,Address(f.data_c));
    assert(roomcats::ReadParentCats(Address(f.image),{22,22}).cats.size()==1);
    assert(roomcats::ReadParentCats(Address(f.image),{0,0}).cats.empty());
    const int reads_before = detail_reads;
    Put(f.data_a,0x70C,int(9)); // Unrelated child's changed data must not be hydrated in the parent view.
    roomcats::ReadParentCats(Address(f.image),{22,33});
    assert(detail_reads == reads_before);
    Put(f.cat_without_room,0x88,std::uint8_t(1));
    Put(f.house, 0x88, Address(f.room_b));
    focused = f.Read();
    assert(focused.valid && focused.cats.size() == 1 && focused.cats[0].id == 22);

    Put(f.house, 0x88, Address(f.empty_room));
    auto empty = f.Read();
    assert(empty.valid && empty.room && empty.cats.empty());
    Put(f.house, 0x88, std::uintptr_t(0));
    all = f.Read();
    assert(all.valid && all.cats.size() == 3);
    Put(f.cat_a, 0xE8, Address(f.box));
    const auto boxed = f.Read();
    assert(boxed.cats[0].location_key == boxed.locations.back().key && boxed.cats[0].location_label == "箱子");
    Put(f.cat_a, 0xE8, Address(f.room_a));
    auto metadata = f.Read(false);
    assert(metadata.in_house && metadata.valid && metadata.cats.empty());
    f.scene[0x4DA] = 1;
    const auto suspended = f.Read();
    assert(suspended.suspended && !suspended.in_house && suspended.scene == Address(f.scene));
    f.scene[0x4DA] = 0;
    assert(f.Read().in_house);

    Put(f.house, 0x88, std::uintptr_t(0x1234));
    auto invalid = f.Read();
    assert(!invalid.valid && invalid.cats.empty() && !invalid.error.empty());
    Put(f.house, 0x88, std::uintptr_t(0));
    f.scene[0x4B0] = 1;
    assert(!f.Read().in_house);
    f.scene[0x4B0] = 0;
    Put(f.cat_b, 0x80, std::uint64_t(11));
    assert(!f.Read().valid);
    roomcats::ConfigureCatDetailsReader(nullptr);
}
