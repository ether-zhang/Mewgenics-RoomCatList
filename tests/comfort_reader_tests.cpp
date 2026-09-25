#include "../src/game_reader.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
struct Effect {
    char name[16]{}; std::uint64_t size=0,capacity=15; double value=0;
    Effect(const char* key,double n):value(n) { size=std::strlen(key); assert(size<=15); std::memcpy(name,key,size); }
};
static_assert(sizeof(Effect)==40);
template<class T> std::uintptr_t Addr(T& v) { return reinterpret_cast<std::uintptr_t>(&v); }
template<class B,class T> void Put(B& b,std::size_t off,const T& value) { assert(off+sizeof(T)<=b.size()); std::memcpy(b.data()+off,&value,sizeof(value)); }
template<class B> void Vector(B& b,std::size_t off,std::uintptr_t begin,std::size_t bytes) {
    Put(b,off,begin); Put(b,off+8,begin+bytes); Put(b,off+16,begin+bytes);
}
std::uintptr_t Effects(std::uintptr_t room) { return room+0x140; }
}
void TestComfortReader() {
    using namespace roomcats;
    std::array<Effect,3> effects{Effect("Comfort",5),Effect("Health",100),Effect("Comfort",-2)};
    std::array<std::uintptr_t,3> vec{Addr(effects),Addr(effects)+sizeof(effects),Addr(effects)+sizeof(effects)};
    double value=0;
    assert(ReadComfortEffect(Addr(vec),value) && value==3);
    ++vec[1]; assert(!ReadComfortEffect(Addr(vec),value)); --vec[1];
    effects[0].value=std::numeric_limits<double>::quiet_NaN(); assert(!ReadComfortEffect(Addr(vec),value));
    vec={0,0,0}; assert(ReadComfortEffect(Addr(vec),value) && value==0);
    vec={0,40,40}; assert(!ReadComfortEffect(Addr(vec),value));
    vec={0,0,200000}; assert(!ReadComfortEffect(Addr(vec),value));
    std::array<unsigned char,0x160> room_a{},room_b{};
    std::array<unsigned char,0xC0> cat{};
    std::array<std::uintptr_t,1> members{Addr(cat)};
    Effect ea("Comfort",21),eb("Comfort",10),ec("Comfort",1);
    Vector(room_a,0x140,Addr(ea),sizeof(ea)); Vector(room_b,0x140,Addr(eb),sizeof(eb)); Vector(cat,0xA0,Addr(ec),sizeof(ec));
    Put(room_a,0x6C,std::uint32_t(1)); Put(room_a,0x70,Addr(members));
    RoomSnapshot s; s.valid=s.in_house=true;
    s.locations={{"room:Attic","Attic",Addr(room_a)},{"room:Floor2_Small","Left",Addr(room_b)}};
    Cat c; c.id=1; c.component=Addr(cat); c.location=Addr(room_a); c.location_key="room:Attic"; s.cats={c};
    ConfigureRoomEffectsReader(nullptr); assert(!ReadPopulationComfort(s));
    ConfigureRoomEffectsReader(Effects); assert(ReadPopulationComfort(s) && s.population_comfort_valid);
    assert(s.cats[0].comfort_effect==1 && s.locations[0].comfort_base==20 && s.locations[1].comfort_base==10);
    // A move changes residents/effect totals but preserves the furniture base.
    s.cats[0].location=Addr(room_b); s.cats[0].location_key="room:Floor2_Small";
    Put(room_a,0x6C,std::uint32_t(0)); Put(room_b,0x6C,std::uint32_t(1)); Put(room_b,0x70,Addr(members)); ea.value=20; eb.value=11;
    assert(ReadPopulationComfort(s) && s.locations[0].comfort_base==20 && s.locations[1].comfort_base==10);
    Put(room_b,0x6C,std::uint32_t(0)); assert(!ReadPopulationComfort(s) && !s.population_comfort_valid);
    Put(room_b,0x6C,std::uint32_t(10001)); assert(!ReadPopulationComfort(s));
    ConfigureRoomEffectsReader(nullptr);
    std::cout << "Comfort reader passed: native effect vectors, resident contributions, stable furniture base, membership validation and invalid-data rejection.\n";
}
