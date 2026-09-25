#include "localization.hpp"
#include "newborn_plan.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <tuple>
#include <unordered_map>

namespace roomcats {
NurseryRoom NurseryOf(const std::string& key) {
    if (key=="room:Attic" || key=="room:SmallAttic") return NurseryRoom::Attic;
    if (key=="room:Floor2_Small") return NurseryRoom::SecondLeft;
    if (key=="room:Floor2_Large") return NurseryRoom::SecondRight;
    if (key=="room:Floor1_Large") return NurseryRoom::FirstLeft;
    if (key=="room:Floor1_Small") return NurseryRoom::Battle;
    return NurseryRoom::Other;
}
int QualityMutations(const CatDetails& d) {
    return static_cast<int>(std::count_if(d.good.begin(),d.good.end(),[](const Trait& t) { return t.quality; }));
}
bool HasCatClass(const CatDetails& d) { return !d.collar.empty() && d.collar!="None" && d.collar!="Colorless"; }
bool ProtectedCat(const Cat& c) { return c.details.marker=="appeal"; } // Native house marker.
bool BreedingHealthRisk(const Cat& c,const BreedingRules& r) { return (r.reject_bad && !c.details.bad.empty()) || (r.reject_disease && !c.details.diseases.empty()); }
bool FailsBattleScreen(const Cat& c,const BattleRules& r) {
    return TotalStats(c.details.real)<r.real_min || (r.reject_lopsided && std::count_if(c.details.real.begin(),c.details.real.end(),[&](auto n) { return n<=r.low_stat_max; })>=r.low_stat_count);
}
namespace {
std::uint64_t Mix(std::uint64_t n) {
    n += 0x9e3779b97f4a7c15ULL; n=(n^(n>>30))*0xbf58476d1ce4e5b9ULL;
    n=(n^(n>>27))*0x94d049bb133111ebULL; return n^(n>>31);
}
void Hash(std::uint64_t& h,std::uint64_t n) { h=Mix(h^Mix(n)); }
void Hash(std::uint64_t& h,const std::string& s) { Hash(h,s.size()); for(unsigned char c:s) Hash(h,c); }
bool Newborn(const Cat& c) { return c.details.age==1; }
bool Inbred(const Cat& c) { return c.details.inbreeding>0.1; }
struct Ranked {
    const NewbornPlan& plan;
    bool dex=false, use_class=true;
    bool operator()(std::size_t a,std::size_t b) const {
        return BetterBreeder(plan.decisions[a].cat,plan.decisions[b].cat,plan.seed,dex && plan.newborn_rules.breeding.prioritize_dex,use_class,plan.newborn_rules.breeding.preferred_dex);
    }
};
}

bool BetterBreeder(const Cat& x,const Cat& y,std::uint64_t seed,bool dex,bool use_class,int preferred_dex) {
    return std::make_tuple(dex && x.details.genetic[1]==preferred_dex,TotalStats(x.details.genetic),QualityMutations(x.details),use_class && HasCatClass(x.details),Mix(x.id^seed),x.id) >
           std::make_tuple(dex && y.details.genetic[1]==preferred_dex,TotalStats(y.details.genetic),QualityMutations(y.details),use_class && HasCatClass(y.details),Mix(y.id^seed),y.id);
}

std::uint64_t NurseryFingerprint(const RoomSnapshot& s) {
    std::uint64_t h=0; Hash(h,s.scene); Hash(h,s.generation); Hash(h,s.game_day); Hash(h,s.cat_database);
    std::vector<const Cat*> cats;
    for(const auto& c:s.cats) if(c.active && !c.record_only) cats.push_back(&c);
    std::sort(cats.begin(),cats.end(),[](auto a,auto b) { return a->id<b->id; });
    for(const auto* c:cats) {
        Hash(h,c->id); Hash(h,c->component); Hash(h,c->generation); Hash(h,c->location_key); Hash(h,c->name);
        Hash(h,c->record_only); Hash(h,c->details.valid); Hash(h,c->details.age); Hash(h,c->details.collar); Hash(h,c->details.marker);
        std::uint64_t coi=0; std::memcpy(&coi,&c->details.inbreeding,sizeof(coi)); Hash(h,coi);
        for(auto n:c->details.genetic) Hash(h,n);
        for(auto n:c->details.real) Hash(h,n);
        for(auto group:{&c->details.good,&c->details.bad,&c->details.diseases}) {
            Hash(h,group->size()); for(const auto& t:*group) { Hash(h,t.name); Hash(h,t.effect); Hash(h,t.quality); }
        }
        Hash(h,c->parents.valid); for(auto id:c->parents.ids) Hash(h,id);
    }
    std::vector<const CatLocation*> locations;
    for(const auto& room:s.locations) locations.push_back(&room);
    std::sort(locations.begin(),locations.end(),[](auto a,auto b) { return std::tie(a->key,a->component,a->generation)<std::tie(b->key,b->component,b->generation); });
    for(const auto* room:locations) { Hash(h,room->key); Hash(h,room->component); Hash(h,room->generation); }
    return h;
}

std::uint64_t ScreeningFingerprint(const RoomSnapshot& s,ScreeningKind kind) {
    auto h=NurseryFingerprint(s);
    if(kind==ScreeningKind::Newborn) return h;
    Hash(h,s.population_comfort_valid);
    std::vector<const CatLocation*> rooms;
    for(const auto& r:s.locations) if(r.kind==LocationKind::Room) rooms.push_back(&r);
    std::sort(rooms.begin(),rooms.end(),[](auto a,auto b) { return a->key<b->key; });
    auto value=[](double v) { return std::isfinite(v) && std::abs(v)<=100000 ? std::uint64_t(std::llround(v*1000000)) : ~0ULL; };
    for(const auto* r:rooms) { Hash(h,r->key); Hash(h,value(r->comfort_base)); }
    std::vector<const Cat*> cats;
    for(const auto& c:s.cats) if(c.active && !c.record_only) cats.push_back(&c);
    std::sort(cats.begin(),cats.end(),[](auto a,auto b) { return a->id<b->id; });
    for(const auto* c:cats) { Hash(h,c->id); Hash(h,value(c->comfort_effect)); }
    return h;
}

NewbornPlan PlanNewbornCats(const RoomSnapshot& s,std::uint64_t seed,NewbornRules rules) {
    NewbornPlan p; p.source=s; p.seed=seed; p.fingerprint=NurseryFingerprint(s); p.newborn_rules=rules;
    ScreeningRules validation; validation.newborn=rules; p.error=ValidateScreeningRules(validation); if(!p.error.empty()) return p;
    if(!s.valid || !s.in_house || s.suspended || s.game_day<0) { p.error=roomcats::Tx("家园数据暂不可用，无法生成方案"); return p; }
    std::array<std::string,5> rooms;
    for(const auto& location:s.locations) {
        const int r=static_cast<int>(NurseryOf(location.key));
        if(r>=0) {
            if(!rooms[r].empty() && rooms[r]!=location.key) { p.error=roomcats::Tx("同一层存在多个房间，无法唯一确定去向"); return p; }
            rooms[r]=location.key;
        }
    }
    for(const auto& key:rooms) if(key.empty()) { p.error=roomcats::Tx("需要顶楼、二楼左右、一楼左右五个房间"); return p; }
    std::unordered_map<std::uint64_t,std::size_t> ids;
    std::vector<int> origin, destination, birth;
    std::vector<bool> parent_demoted;
    for(const auto& cat:s.cats) {
        const int r=static_cast<int>(NurseryOf(cat.location_key));
        if(r<0 || cat.record_only || !cat.active) continue;
        if(ids.count(cat.id)) { p.error=roomcats::Tx("猫咪 ID 重复，无法生成可靠方案"); return p; }
        if(!ProtectedCat(cat) && (!cat.details.valid || (r<4 && Newborn(cat) && cat.details.inbreeding<0))) { p.error=cat.name+roomcats::Tx(" 的属性或近亲数据不完整，请刷新后再筛选"); return p; }
        ids[cat.id]=p.decisions.size();
        NewbornDecision d; d.cat=cat; d.destination=cat.location_key;
        int born=r;
        if(Newborn(cat) && r<4 && !ProtectedCat(cat)) {
            constexpr const char* marks[]={"triangle","square","circle","sword"};
            for(int b=0;b<4;++b) if(cat.details.marker==marks[b]) born=b;
            if(rules.mark_birth) d.marker=marks[born]; ++p.newborns;
        }
        p.decisions.push_back(std::move(d)); origin.push_back(r); destination.push_back(r); birth.push_back(born); parent_demoted.push_back(false);
    }
    auto move=[&](std::size_t i,int to,const std::string& why) {
        destination[i]=to; p.decisions[i].destination=rooms[to];
        if(!p.decisions[i].reason.empty()) p.decisions[i].reason+="；";
        p.decisions[i].reason+=why;
    };
    auto balanced_second=[&]() {
        const auto left=std::count(destination.begin(),destination.end(),1);
        const auto right=std::count(destination.begin(),destination.end(),2);
        return left<right ? 1 : right<left ? 2 : (Mix(seed++)&1 ? 1 : 2);
    };
    // First separate inbred parents, once per pair. Parents already in
    // different rooms cannot reproduce together and need no second demotion.
    std::set<std::pair<std::uint64_t,std::uint64_t>> pairs;
    for(std::size_t i=0;i<p.decisions.size();++i) {
        const auto& c=p.decisions[i].cat;
        if(origin[i]>=4 || !Newborn(c) || !Inbred(c) || ProtectedCat(c)) continue;
        if(rules.split_parents) {
            const auto a=c.parents.ids[0],b=c.parents.ids[1];
            if(!c.parents.valid || !a || !b) { p.error=c.name+roomcats::Tx(" 的近亲父母记录不完整，无法决定亲本降级"); return p; }
            if(pairs.insert(std::minmax(a,b)).second) {
                if(ids.count(a) && ids.count(b)) {
                    const auto ia=ids.at(a),ib=ids.at(b);
                    if(origin[ia]<4 && origin[ia]==origin[ib] && destination[ia]==destination[ib]) {
                        if(ProtectedCat(p.decisions[ia].cat) && ProtectedCat(p.decisions[ib].cat)) {
                            p.notes.push_back(c.name+roomcats::Tx(" 的双亲均有房子标记，保留亲本位置"));
                            move(i,4,roomcats::Tx("近亲小猫转备战")); continue;
                        }
                        const auto weak=ProtectedCat(p.decisions[ia].cat) ? ib : ProtectedCat(p.decisions[ib].cat) ? ia : Ranked{p,false}(ia,ib) ? ib : ia;
                        if(!parent_demoted[weak] && (origin[weak]!=3 || rules.demote_lowest_parent)) {
                            const int to=origin[weak]==0 ? balanced_second() : origin[weak]<3 ? 3 : 4;
                            move(weak,to,roomcats::Tx("近亲亲本降一级")); parent_demoted[weak]=true;
                        }
                    }
                } else p.notes.push_back(c.name+roomcats::Tx(" 的父母已不同时在繁育房，无需重复拆分"));
            }
        }
        move(i,4,roomcats::Tx("近亲小猫转备战"));
    }
    for(std::size_t i=0;i<p.decisions.size();++i) {
        const auto& c=p.decisions[i].cat;
        if(origin[i]<4 && Newborn(c) && !ProtectedCat(c) && BreedingHealthRisk(c,rules.breeding)) move(i,4,roomcats::Tx("有坏变异或疾病，转备战筛选"));
    }
    auto candidates=[&](auto accept) {
        std::vector<std::size_t> result;
        for(std::size_t i=0;i<p.decisions.size();++i)
            if(!parent_demoted[i] && Newborn(p.decisions[i].cat) && !ProtectedCat(p.decisions[i].cat) && !BreedingHealthRisk(p.decisions[i].cat,rules.breeding) && !Inbred(p.decisions[i].cat) && accept(i)) result.push_back(i);
        return result;
    };
    // At most two promotions per original floor, one level per run. They
    // compete for the same upper-floor newborn slots as local kittens.
    auto up=candidates([&](auto i) { return (origin[i]==1 || origin[i]==2) && rules.promote && TotalStats(p.decisions[i].cat.details.genetic)>=rules.breeding.genetic_min[0]; });
    std::sort(up.begin(),up.end(),Ranked{p,true});
    for(std::size_t n=0;n<std::min<std::size_t>(rules.promotion_limit,up.size());++n) move(up[n],0,roomcats::Tx("达到顶楼门槛，参加顶楼筛选"));
    up=candidates([&](auto i) { return origin[i]==3 && rules.promote && TotalStats(p.decisions[i].cat.details.genetic)>=rules.breeding.genetic_min[1]; });
    std::sort(up.begin(),up.end(),Ranked{p});
    for(std::size_t n=0;n<std::min<std::size_t>(rules.promotion_limit,up.size());++n) move(up[n],balanced_second(),roomcats::Tx("达到二楼门槛，晋升一级"));
    auto attic=candidates([&](auto i) { return destination[i]==0; });
    std::sort(attic.begin(),attic.end(),Ranked{p,true});
    int kept=0, spill=0; const int first=balanced_second();
    for(auto i:attic) {
        const auto total=TotalStats(p.decisions[i].cat.details.genetic);
        if(total>=rules.breeding.genetic_min[0] && kept<rules.attic_limit) { ++kept; continue; }
        move(i,spill++%2 ? 3-first : first,total<rules.breeding.genetic_min[0] ? roomcats::Tx("顶楼遗传低于")+std::to_string(rules.breeding.genetic_min[0])+roomcats::Tx("，下放二楼") : roomcats::Tx("顶楼新生名额最多")+std::to_string(rules.attic_limit)+roomcats::Tx("只，下放二楼"));
    }
    auto second=candidates([&](auto i) { return destination[i]==1 || destination[i]==2; });
    std::sort(second.begin(),second.end(),Ranked{p});
    std::vector<std::size_t> keep;
    for(auto i:second) {
        if(TotalStats(p.decisions[i].cat.details.genetic)>=rules.breeding.genetic_min[1] && keep.size()<std::size_t(rules.second_limit*2)) keep.push_back(i);
        else move(i,3,TotalStats(p.decisions[i].cat.details.genetic)<rules.breeding.genetic_min[1] ? roomcats::Tx("二楼遗传低于")+std::to_string(rules.breeding.genetic_min[1])+roomcats::Tx("，下放一楼左") : roomcats::Tx("二楼新生名额每房最多")+std::to_string(rules.second_limit)+roomcats::Tx("只，下放一楼左"));
    }
    // The configured cap is bounded to 12 candidates. Preserve the best cats,
    // then prefer the requested local/cross-room mix within each room limit.
    std::tuple<int,int,std::int64_t,std::uint64_t> best{999,999,999999,~0ULL}; unsigned chosen=0;
    for(unsigned mask=0;mask<(1u<<keep.size());++mask) {
        int counts[2]{},local[2]{},cross[2]{}; std::int64_t genetics[2]{};
        for(std::size_t k=0;k<keep.size();++k) {
            const int side=(mask>>k)&1; const auto i=keep[k]; ++counts[side];
            local[side]+=birth[i]==side+1; cross[side]+=birth[i]==2-side;
            genetics[side]+=TotalStats(p.decisions[i].cat.details.genetic);
        }
        if(counts[0]>rules.second_limit || counts[1]>rules.second_limit) continue;
        const int quota=std::abs(local[0]-(rules.second_limit-rules.second_cross_target))+std::abs(local[1]-(rules.second_limit-rules.second_cross_target))+std::abs(cross[0]-rules.second_cross_target)+std::abs(cross[1]-rules.second_cross_target);
        const auto score=std::make_tuple(std::abs(counts[0]-counts[1]),quota,std::abs(genetics[0]-genetics[1]),Mix(mask^p.seed));
        if(score<best) { best=score; chosen=mask; }
    }
    for(std::size_t k=0;k<keep.size();++k) {
        const auto i=keep[k]; const int to=1+((chosen>>k)&1);
        if(destination[i]!=to) move(i,to,roomcats::Tx("二楼新生交换和平衡分配"));
    }
    for(auto i:candidates([&](auto j) { return destination[j]==3; }))
        if(TotalStats(p.decisions[i].cat.details.genetic)<rules.breeding.genetic_min[2]) move(i,4,roomcats::Tx("一楼左遗传低于")+std::to_string(rules.breeding.genetic_min[2])+roomcats::Tx("，转备战"));
    for(std::size_t i=0;i<p.decisions.size();++i) if(destination[i]==4 && !ProtectedCat(p.decisions[i].cat)) {
        auto& d=p.decisions[i]; const auto total=TotalStats(d.cat.details.real);
        const int low=static_cast<int>(std::count_if(d.cat.details.real.begin(),d.cat.details.real.end(),[&](auto n) { return n<=rules.battle.low_stat_max; }));
        const bool class_parent=rules.class_parent_no_battle && origin[i]<4 && d.cat.details.age>1 && HasCatClass(d.cat.details);
        if(class_parent || FailsBattleScreen(d.cat,rules.battle)) {
            d.discard=true;
            if(!d.reason.empty()) d.reason+="；";
            if(class_parent) { d.destination=d.cat.location_key; d.reason+=roomcats::Tx("带职业成年亲本不转战备，进入淘汰复查"); }
            else d.reason+=roomcats::Tx("备战真实总和 ")+std::to_string(total)+roomcats::Tx("（下限")+std::to_string(rules.battle.real_min)+roomcats::Tx("），真实≤")+std::to_string(rules.battle.low_stat_max)+roomcats::Tx("属性 ")+std::to_string(low)+roomcats::Tx(" 项");
        }
    }
    return p;
}

bool ValidateNewbornPlan(const NewbornPlan& p,const RoomSnapshot& fresh,std::string& reason) {
    if(!p.Valid() || !fresh.valid || !fresh.in_house || fresh.suspended ||
        (p.kind==ScreeningKind::Adult && !fresh.population_comfort_valid) || p.fingerprint!=ScreeningFingerprint(fresh,p.kind)) {
        reason=roomcats::Tx("猫咪、房间或日期已变化，请重新生成筛选方案后确认"); return false;
    }
    reason.clear(); return true;
}
}
