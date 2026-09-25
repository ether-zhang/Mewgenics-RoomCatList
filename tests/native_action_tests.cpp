// Runs native-action orchestration against synthetic memory and substitute
// engine callbacks. Does not attach to or operate a running game.
#include "../src/native_actions.cpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
template<std::size_t N> using Bytes = std::array<unsigned char, N>;
template<class T> void Write(std::uintptr_t base, unsigned offset, T value) { std::memcpy(reinterpret_cast<void*>(base+offset), &value, sizeof(value)); }
template<class T> std::uintptr_t Address(T& x) { return reinterpret_cast<std::uintptr_t>(x.data()); }
struct Fixture {
    std::vector<unsigned char> image = std::vector<unsigned char>(0x1400000);
    Bytes<0x600> director{}, scene_memory{}, progress{};
    Bytes<0x138> manager{};
    Bytes<0x208> cat_memory{}, pipe_memory{}, map_memory{}, drawer_memory{}, ui_memory{}, room_memory{};
    Bytes<0xC58> data{};
    Bytes<0x20> cache_node{}, sentinel{};
    std::array<std::uintptr_t, 2> buckets{};
    std::array<std::uintptr_t, 6> components{};
    std::array<std::uintptr_t, 1> pipe_slots{};
    Bytes<16> component_list{};
    roomcats::RoomSnapshot snapshot;
    roomcats::CatAction action;
    std::uintptr_t cat, pipe, map, drawer, ui, room;
    unsigned next_type = 0x10000;
    void Type(std::uintptr_t obj, const char* name) {
        auto rva = next_type; next_type += 0x200;
        const auto base = Address(image);
        std::memcpy(image.data()+rva+16, name, std::strlen(name)+1);
        Write(base, rva+0x80, unsigned(1)); Write(base,rva+0x8C,rva); Write(base,rva+0x94,rva+0x80);
        Write(base,rva+0xA8,base+rva+0x80); Write(obj,0,base+rva+0xB0);
        Write(obj,0x20,snapshot.scene); Write(obj-8,0,std::uint64_t(77));
    }
    Fixture() {
        snapshot.valid = snapshot.in_house = true;
        snapshot.scene = Address(scene_memory)+8; snapshot.generation = 77;
        cat = Address(cat_memory)+8; pipe = Address(pipe_memory)+8; map = Address(map_memory)+8;
        drawer = Address(drawer_memory)+8; ui = Address(ui_memory)+8; room = Address(room_memory)+8;
        Type(cat,".?AVHouseCat@glaiel@@"); Type(pipe,".?AVHousePipe@glaiel@@"); Type(map,".?AVNPCMapDrawer@glaiel@@");
        Type(drawer,".?AVCatStatsDrawer@glaiel@@"); Type(ui,".?AVHouseDrawerUI@glaiel@@"); Type(room,".?AVFurnitureGrid@glaiel@@");
        snapshot.pipe = pipe; snapshot.npc_drawer = map; snapshot.cat_drawer = drawer; snapshot.drawer_ui = ui;
        components = {cat,pipe,map,drawer,ui,room};
        Write(Address(component_list),0,unsigned(6)); Write(Address(component_list),4,unsigned(6));
        Write(Address(component_list),8,Address(components)); Write(snapshot.scene,0x18,Address(component_list));
        Write(Address(image),0x13DAC30,Address(director));
        Write(Address(director),0x598,Address(manager)); Write(Address(director),0x5A8,Address(progress));
        buckets = {Address(cache_node),Address(cache_node)};
        Write(Address(manager),0xF0,Address(sentinel)); Write(Address(manager),0x100,Address(buckets));
        Write(Address(cache_node),0x10,std::uint64_t(42)); Write(Address(cache_node),0x18,Address(data));
        Write(Address(data),0xC48,std::uint64_t(42));
        Write(cat,0x80,std::uint64_t(42)); Write(cat,0x88,static_cast<unsigned char>(1)); Write(cat,0xE8,room);
        Write(pipe,0x100,Address(pipe_slots)); Write(pipe,0x110,Address(pipe_slots)); Write(pipe,0xE8,std::uintptr_t(100));
        Write(drawer,0x38,std::uintptr_t(500)); Write(map,0x38,std::uintptr_t(600)); Write(map,0xB8,int(8));
        for (int npc : {0,1,7}) Write(Address(progress),0x48+npc*0x78+0x50,static_cast<unsigned char>(1));
        action.cat = 42; action.component = cat; action.cat_generation = 77; action.source = room;
        action.scene = snapshot.scene; action.scene_generation = 77; action.npc = 0; action.kind = roomcats::CatActionKind::Donate;
        roomcats::g_action_image = Address(image);
    }
};
Fixture* fixture = nullptr;
int placed = 0, clicked = 0, opened = 0;
int discarded = 0;
bool accepts = true;
int marked = 0;
int refreshed = 0;
void __fastcall Refresh(void* drawer,void* cat,unsigned char force) {
    ++refreshed; assert(std::uintptr_t(drawer)==fixture->drawer && std::uintptr_t(cat)==fixture->cat && force==0);
    assert(roomcats::Value<unsigned char>(fixture->drawer+0x70)==1);
    Write(fixture->drawer,0x70,static_cast<unsigned char>(0));
}
void* __fastcall AssignMarker(void* dest,const char* text,std::size_t size) {
    ++marked; assert(size<16); std::memset(dest,0,32); std::memcpy(dest,text,size);
    Write(std::uintptr_t(dest),16,std::uint64_t(size)); Write(std::uintptr_t(dest),24,std::uint64_t(15)); return dest;
}
unsigned char __fastcall Accept(void*, int npc, void* data) { assert(data == fixture->data.data()); return accepts && npc == 0; }
void __fastcall Place(void* pipe, void* cat) {
    ++placed;
    assert(std::uintptr_t(pipe) == fixture->pipe && std::uintptr_t(cat) == fixture->cat);
    Write(fixture->cat,0xE8,fixture->pipe); fixture->pipe_slots[0] = fixture->cat;
}
void __fastcall Click(void* map) { ++clicked; assert(std::uintptr_t(map) == fixture->map); }
void __fastcall Discard(void* closure) { ++discarded; assert(static_cast<std::uintptr_t*>(closure)[1] == fixture->map); }
void __fastcall Open(void* drawer, void* cat, unsigned char force) {
    ++opened; assert(std::uintptr_t(drawer) == fixture->drawer && std::uintptr_t(cat) == fixture->cat && force == 1);
    Write(fixture->drawer,0x78,fixture->cat); Write(fixture->ui,0x58,std::uintptr_t(500));
}
}

int main() {
    roomcats::SetLanguage(roomcats::Language::Chinese);
    using namespace roomcats;
    Fixture f; fixture = &f;
    g_accept_npc = Accept; g_place_in_pipe = Place; g_open_cat = Open; g_npc_clicks.fill(Click);
    g_discard_click = Discard;
    Cat c; c.id = 42; c.component = f.cat; c.generation = 77; c.location = f.room;
    const auto before = f.data;
    const auto choices = QueryNpcs(f.snapshot,c);
    assert(choices.size() == 4 && choices[0].enabled && !choices[1].enabled && !choices[2].enabled);
    assert(choices[3].npc == kDiscardDestination && choices[3].enabled && choices[3].label == "遗弃");
    assert(choices[2].npc == 7 && f.data == before && placed == 0 && clicked == 0);
    // Native HousePipe remembers occupancy from its previous update. The
    // slot is already empty but another cat must wait for this latch to reset.
    Write(f.pipe,0xF0,int(1));
    auto resetting=QueryNpcs(f.snapshot,c);
    assert(!resetting[0].enabled && resetting[0].transient && resetting[3].transient && placed==0);
    assert(!resetting[1].transient); // A real NPC rejection is not retried.
    Write(f.pipe,0xF0,int(0));
    Write(f.ui,0x58,std::uintptr_t(600)); Write(f.map,0x48,std::uintptr_t(0));
    auto closing=QueryNpcs(f.snapshot,c);
    assert(!closing[3].enabled && closing[3].transient);
    Write(f.ui,0x58,std::uintptr_t(0));
    assert(QueryNpcs(f.snapshot,c)[3].enabled);
    std::string message;
    auto a = f.action;
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Running && a.phase == 1 && placed == 1);
    for (int i=0;i<10;++i) assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Running);
    assert(placed == 1 && clicked == 0);
    Write(f.map,0x40,static_cast<unsigned char>(1)); Write(f.map,0x48,f.cat); Write(f.map,0x50,std::uint64_t(77)); Write(f.ui,0x58,std::uintptr_t(600));
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Running && clicked == 0); // Menu animation not ready.
    Write(f.map,0x41,static_cast<unsigned char>(1));
    Write(f.map,0x40,static_cast<unsigned char>(0)); // Native initialization consumes its opening request.
    accepts = false;
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Failed && clicked == 0);
    accepts = true;
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Running && a.phase == 2 && clicked == 1);
    for (int i=0;i<10;++i) assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Running);
    assert(clicked == 1 && placed == 1);
    Write(f.map,0x48,std::uintptr_t(0)); Write(f.map,0xB8,int(0));
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Failed); // Closing menu is not proof of donation.
    Write(f.cat,0x88,static_cast<unsigned char>(0));
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Complete);
    Write(f.ui,0x58,std::uintptr_t(0));
    Write(f.cat,0x88,static_cast<unsigned char>(1));
    a = f.action; a.kind = CatActionKind::Inspect;
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Complete && opened == 1);
    a.cat_generation++;
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Failed && opened == 1);
    assert(f.data == before);
    a = f.action; a.npc = kDiscardDestination;
    Write(f.cat,0xE8,f.room); f.pipe_slots[0] = 0; Write(f.ui,0x58,std::uintptr_t(0));
    accepts = false; // Discard uses the original trash menu, not NPC progress indexing.
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Running && a.phase == 1);
    Write(f.map,0x48,f.cat); Write(f.map,0x50,std::uint64_t(77)); Write(f.ui,0x58,std::uintptr_t(600));
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Running && discarded == 1 && clicked == 1);
    for (int i=0;i<10;++i) StepNativeAction(f.snapshot,a,message);
    assert(discarded == 1);
    Write(f.map,0x48,std::uintptr_t(0)); Write(f.map,0xB8,int(8)); Write(f.cat,0x88,static_cast<unsigned char>(0));
    assert(StepNativeAction(f.snapshot,a,message) == CatActionStatus::Complete && message == "已遗弃");
    Write(f.cat,0x88,static_cast<unsigned char>(1)); Write(f.cat,0xE8,f.room);
    Write(Address(f.director),0x580,int(42)); Write(Address(f.data),0xC38,std::int64_t(41));
    g_assign_marker=AssignMarker; g_refresh_cat=Refresh; Write(f.ui,0x58,std::uintptr_t(500));
    a=f.action; a.kind=CatActionKind::Mark; a.marker="triangle";
    assert(StepNativeAction(f.snapshot,a,message)==CatActionStatus::Complete && marked==1 && refreshed==1);
    std::string symbol; assert(ReadNarrowString(Address(f.data)+0x38,symbol) && symbol=="triangle");
    a.marker="unsupported"; assert(StepNativeAction(f.snapshot,a,message)==CatActionStatus::Failed && marked==1);
    a.marker="circle"; Write(Address(f.director),0x580,int(43));
    assert(StepNativeAction(f.snapshot,a,message)==CatActionStatus::Failed && marked==1);
    std::cout << "Native actions passed: unlocked/disabled NPCs, preview immutability, pipe/menu/donation sequencing, revalidation, no duplicate click, verified completion and original cat panel call.\n";
}
