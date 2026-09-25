#pragma once
#include "../src/newborn_plan.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK_NEWBORN(x) do { if (!(x)) { std::cerr << "Newborn check failed, line " << __LINE__ << ": " << #x << "\n"; return 1; } } while(false)
namespace newborn_test {
inline roomcats::RoomSnapshot House() {
    roomcats::RoomSnapshot s; s.valid=s.in_house=true; s.scene=1234; s.generation=12; s.game_day=42;
    s.cat_database=5678;
    const char* rooms[]={"Attic","Floor2_Small","Floor2_Large","Floor1_Large","Floor1_Small"};
    for(int i=0;i<5;++i) s.locations.push_back({std::string("room:")+rooms[i],rooms[i],std::uintptr_t(100+i),1,roomcats::LocationKind::Room});
    s.locations.push_back({"box:999","Box",999,1,roomcats::LocationKind::Box}); return s;
}
inline roomcats::Cat MakeCat(std::uint64_t id,int room,int genetics,int real=56,int age=1,int quality=0,int dex=7) {
    auto s=House(); roomcats::Cat c; c.id=id; c.component=2000+id; c.generation=3; c.name="Cat "+std::to_string(id);
    c.location=s.locations[room].component; c.location_key=s.locations[room].key; c.location_label=s.locations[room].label;
    c.details.valid=true; c.details.age=age; c.details.inbreeding=0; c.details.collar="Colorless";
    c.details.genetic.fill(7); c.details.genetic[0]+=genetics-49; c.details.genetic[1]=dex; c.details.genetic[0]+=7-dex;
    c.details.real.fill(7); c.details.real[0]+=real-49;
    for(int i=0;i<quality;++i) c.details.good.push_back({"Good","Positive effect",{},true});
    return c;
}
inline const roomcats::NewbornDecision& Decision(const roomcats::NewbornPlan& p,std::uint64_t id) {
    for(const auto& d:p.decisions) if(d.cat.id==id) return d;
    throw std::runtime_error("Missing decision");
}
inline int Room(const roomcats::NewbornDecision& d) { return int(roomcats::NurseryOf(d.destination)); }
}
