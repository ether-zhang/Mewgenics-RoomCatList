#include "localization.hpp"
#include "adult_plan.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>

namespace roomcats {
double RoomComfort(double effects,int cats) { return effects-std::max(0,cats-4); }

PopulationProjection ProjectPopulation(const NewbornPlan& p) {
    PopulationProjection out;
    for(const auto& room:p.source.locations) {
        const int r=int(NurseryOf(room.key)); if(r>=0) out.comfort[r]=room.comfort_base;
    }
    for(const auto& cat:p.source.cats) if(cat.active && !cat.record_only) {
        auto key=cat.location_key;
        const auto it=std::find_if(p.decisions.begin(),p.decisions.end(),[&](const NewbornDecision& d) { return d.cat.id==cat.id; });
        if(it!=p.decisions.end()) {
            if(!it->discard) key=it->destination;
            else if(it->choice==ReviewChoice::Location) key=it->alternative_location;
            else if(it->choice!=ReviewChoice::Keep) continue;
        }
        ++out.total; out.protected_cats+=ProtectedCat(cat);
        const int r=int(NurseryOf(key));
        if(r>=0) { ++out.counts[r]; out.comfort[r]+=cat.comfort_effect; }
    }
    for(int r=0;r<5;++r) out.comfort[r]=RoomComfort(out.comfort[r],out.counts[r]);
    return out;
}

NewbornPlan PlanAdultCats(const RoomSnapshot& s,std::uint64_t seed,AdultRules rules) {
    NewbornPlan p; p.source=s; p.seed=seed; p.kind=ScreeningKind::Adult; p.adult_rules=rules;
    ScreeningRules validation; validation.adult=rules; p.error=ValidateScreeningRules(validation); if(!p.error.empty()) return p;
    p.fingerprint=ScreeningFingerprint(s,p.kind);
    if(!s.valid || !s.in_house || s.suspended || s.game_day<0 || !s.population_comfort_valid) {
        p.error=roomcats::Tx("家园或舒适度数据尚未就绪，请稍后重新生成老猫方案"); return p;
    }
    std::array<std::string,5> rooms;
    std::array<double,5> effects{};
    std::array<int,5> counts{};
    for(const auto& room:s.locations) {
        const int r=int(NurseryOf(room.key)); if(r<0) continue;
        if(!rooms[r].empty() || !std::isfinite(room.comfort_base) || std::abs(room.comfort_base)>100000) {
            p.error=roomcats::Tx("房间或舒适度数据不完整，无法生成老猫方案"); return p;
        }
        rooms[r]=room.key; effects[r]=room.comfort_base;
    }
    for(const auto& key:rooms) if(key.empty()) { p.error=roomcats::Tx("需要顶楼、二楼左右、一楼左右五个房间"); return p; }
    std::set<std::uint64_t> ids;
    std::vector<int> origin,destination;
    std::vector<std::size_t> adults,breeders;
    for(const auto& cat:s.cats) if(cat.active && !cat.record_only) {
        const int r=int(NurseryOf(cat.location_key));
        if(!ids.insert(cat.id).second || !std::isfinite(cat.comfort_effect) || std::abs(cat.comfort_effect)>100000) {
            p.error=roomcats::Tx("猫咪或房间效果数据不完整，无法生成老猫方案"); return p;
        }
        if(r>=0 && !ProtectedCat(cat) && (!cat.details.valid || (cat.details.age>1 && r<4 &&
            (!std::isfinite(cat.details.inbreeding) || cat.details.inbreeding<0)))) {
            p.error=cat.name+roomcats::Tx(" 的属性或近亲数据不完整，请刷新后重试"); return p;
        }
        const auto i=p.decisions.size();
        NewbornDecision d; d.cat=cat; d.destination=cat.location_key;
        p.decisions.push_back(std::move(d)); origin.push_back(r); destination.push_back(r);
        if(r>=0) { ++counts[r]; effects[r]+=cat.comfort_effect; }
        if(r>=0 && cat.details.age>1 && !ProtectedCat(cat)) adults.push_back(i);
    }
    auto comfort=[&](int r) { return RoomComfort(effects[r],counts[r]); };
    auto relocate=[&](std::size_t i,int to) {
        const int from=destination[i]; if(from==to) return;
        const auto delta=p.decisions[i].cat.comfort_effect;
        if(from>=0) { --counts[from]; effects[from]-=delta; }
        if(to>=0) { ++counts[to]; effects[to]+=delta; }
        destination[i]=to;
        if(to>=0) p.decisions[i].destination=rooms[to];
    };
    for(auto i:adults) if(origin[i]<4) {
        const auto& c=p.decisions[i].cat;
        relocate(i,4);
        if(BreedingHealthRisk(c,rules.breeding)) p.decisions[i].reason=roomcats::Tx("有坏变异或疾病，转备战筛选");
        else if(c.details.inbreeding>(rules.allow_mild_first ? 0.25 : 0.1)) p.decisions[i].reason=roomcats::Tx("近亲程度超过允许范围，转备战筛选");
        else breeders.push_back(i);
    }
    auto rank=[&](std::size_t a,std::size_t b,bool dex=false) { return BetterBreeder(p.decisions[a].cat,p.decisions[b].cat,seed,dex && rules.breeding.prioritize_dex,true,rules.breeding.preferred_dex); };
    auto eligible=[&](std::size_t i,int threshold,bool mild) {
        const auto& c=p.decisions[i].cat;
        return destination[i]==4 && TotalStats(c.details.genetic)>=threshold && (mild || c.details.inbreeding<=0.1);
    };
    // Fill a tier with its best eligible adults, then remove its weakest
    // contributors to crowding. Positive comfort auras travel with their cat.
    auto trim=[&](int r) {
        std::vector<std::size_t> in_room;
        for(auto i:breeders) if(destination[i]==r) in_room.push_back(i);
        std::sort(in_room.begin(),in_room.end(),[&](auto a,auto b) { return rank(b,a,r==0); });
        while(comfort(r)+1e-6<rules.comfort_min[r]) {
            auto it=std::find_if(in_room.begin(),in_room.end(),[&](auto i) {
                return RoomComfort(effects[r]-p.decisions[i].cat.comfort_effect,counts[r]-1)>comfort(r)+1e-6;
            });
            if(it==in_room.end()) break; // Protected/young residents or furniture make this floor unattainable.
            relocate(*it,4); in_room.erase(it);
        }
    };
    for(auto i:breeders) if(eligible(i,rules.breeding.genetic_min[0],false)) relocate(i,0);
    trim(0);
    std::sort(breeders.begin(),breeders.end(),[&](auto a,auto b) { return rank(a,b); });
    for(auto i:breeders) if(eligible(i,rules.breeding.genetic_min[1],false)) {
        const auto delta=p.decisions[i].cat.comfort_effect;
        const auto left=RoomComfort(effects[1]+delta,counts[1]+1),right=RoomComfort(effects[2]+delta,counts[2]+1);
        const int side=left-rules.comfort_min[1]>right-rules.comfort_min[2]+1e-6 ? 1 : right-rules.comfort_min[2]>left-rules.comfort_min[1]+1e-6 ? 2 : counts[1]<counts[2] ? 1 : counts[2]<counts[1] ? 2 : origin[i]==2 ? 2 : 1;
        relocate(i,side);
    }
    trim(1); trim(2);
    // Unequal furniture/aura effects can leave space on the opposite side.
    for(auto i:breeders) if(eligible(i,rules.breeding.genetic_min[1],false)) {
        int side=comfort(1)-rules.comfort_min[1]>=comfort(2)-rules.comfort_min[2] ? 1 : 2;
        if(RoomComfort(effects[side]+p.decisions[i].cat.comfort_effect,counts[side]+1)+1e-6>=rules.comfort_min[side]) relocate(i,side);
    }
    for(auto i:breeders) if(eligible(i,rules.breeding.genetic_min[2],rules.allow_mild_first)) relocate(i,3);
    trim(3);
    for(auto i:adults) {
        auto& d=p.decisions[i];
        if(destination[i]!=origin[i] && d.reason.empty())
            d.reason=roomcats::Tx("遗传 ")+std::to_string(TotalStats(d.cat.details.genetic))+roomcats::Tx("，按品质与舒适度分配至 ")+RoomLabel(rooms[destination[i]].substr(5));
        const bool class_rejected=rules.class_no_battle && origin[i]<4 && destination[i]==4 && HasCatClass(d.cat.details);
        if(destination[i]==4 && (class_rejected || FailsBattleScreen(d.cat,rules.battle))) {
            if(!d.reason.empty()) d.reason+="；";
            if(class_rejected) { d.destination=d.cat.location_key; d.reason=roomcats::Tx("带职业老猫未能留在繁育房，不转战备，进入淘汰复查"); }
            else d.reason+=roomcats::Tx("备战真实总和低于")+std::to_string(rules.battle.real_min)+(rules.battle.reject_lopsided ? roomcats::Tx("或至少")+std::to_string(rules.battle.low_stat_count)+roomcats::Tx("项真实值≤")+std::to_string(rules.battle.low_stat_max) : "");
            d.discard=true; relocate(i,-1);
        }
    }
    int total=static_cast<int>(p.decisions.size())-static_cast<int>(std::count_if(p.decisions.begin(),p.decisions.end(),[](const auto& d) { return d.discard; }));
    // Preserve the stronger breeding tiers. Battle cats are ranked exclusively
    // by current stats, quality abilities and class, never genetic potential.
    auto tier=[](int r) { return r==2 ? 1 : r; };
    auto combat_better=[&](std::size_t a,std::size_t b) {
        const auto& x=p.decisions[a].cat; const auto& y=p.decisions[b].cat;
        const auto sx=std::make_tuple(TotalStats(x.details.real),QualityMutations(x.details),HasCatClass(x.details));
        const auto sy=std::make_tuple(TotalStats(y.details.real),QualityMutations(y.details),HasCatClass(y.details));
        if(sx!=sy) return sx>sy;
        // Reuse the seeded tie break without allowing genes to affect combat rank.
        auto cx=x,cy=y; cx.details.genetic.fill(0); cy.details.genetic.fill(0);
        return BetterBreeder(cx,cy,seed);
    };
    std::vector<std::size_t> cull;
    for(auto i:adults) if(!p.decisions[i].discard) cull.push_back(i);
    std::sort(cull.begin(),cull.end(),[&](auto a,auto b) {
        if(tier(destination[a])!=tier(destination[b])) return tier(destination[a])>tier(destination[b]);
        return destination[a]==4 ? combat_better(b,a) : rank(b,a,destination[a]==0);
    });
    for(auto i:cull) {
        if(total<=rules.population_target) break;
        const int r=destination[i];
        if(r<4 && RoomComfort(effects[r]-p.decisions[i].cat.comfort_effect,counts[r]-1)+1e-6 <
            std::min<double>(rules.comfort_min[r],comfort(r))) continue;
        auto& d=p.decisions[i]; d.discard=true;
        if(!d.reason.empty()) d.reason+="；";
        d.reason+=roomcats::Tx("总数控制在")+std::to_string(rules.population_target)+roomcats::Tx("只，按所在层级与能力择弱淘汰");
        relocate(i,-1); --total;
    }
    p.notes.push_back(roomcats::Tx("只调整年龄大于1的猫；房子标记始终保留。幼猫、箱子及其他位置计入总数但不自动调动。"));
    p.notes.push_back(roomcats::Tx("繁育房按遗传属性、优质变异、有无职业比较；战备按真实属性、优质能力、有无职业比较。"));
    if(rules.breeding.prioritize_dex) p.notes.push_back(roomcats::Tx("顶楼优先遗传敏捷为")+std::to_string(rules.breeding.preferred_dex)+roomcats::Tx("的猫。"));
    if(rules.class_no_battle) p.notes.push_back(roomcats::Tx("繁育房淘汰的带职业老猫不转战备，直接进入淘汰复查；原本在战备房的猫仍按真实能力筛选。"));
    if(total>rules.population_target) p.notes.push_back(roomcats::Tx("受保护猫、幼猫或舒适度限制，当前方案无法降至")+std::to_string(rules.population_target)+roomcats::Tx("只；不会扩大淘汰范围。"));
    for(int r=0;r<4;++r) if(comfort(r)+1e-6<rules.comfort_min[r])
        p.notes.push_back(RoomLabel(rooms[r].substr(5))+roomcats::Tx(" 受固定住猫或家具限制，无法达到舒适度目标。"));
    return p;
}
}
