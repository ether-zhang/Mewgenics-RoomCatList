#pragma once
#include "newborn_plan.hpp"
#include "cat_actions.hpp"
#include <array>
#include <unordered_map>

namespace roomcats {
// Review preferences only. No actor pointers, commands, or old plan are reused.
// Lives outside the panel/view state so closing it or an NPC cutscene cannot
// erase choices. Newborn/adult choices are independent within one save/day.
class ScreeningChoices {
    struct Selection {
        ReviewChoice choice;
        std::string location,label;
        int npc;
    };
    std::uintptr_t database_=0;
    int day_=-1;
    std::array<std::unordered_map<std::uint64_t,Selection>,2> cats_;
    bool Matches(const RoomSnapshot& s) const {
        return s.cat_database && database_==s.cat_database && day_==s.game_day;
    }
public:
    void Observe(const RoomSnapshot& s) {
        // Missing/suspended house data is a temporary transition, not a reset.
        if(!s.valid || !s.in_house || s.suspended || !s.cat_database || s.game_day<0) return;
        if(!Matches(s)) {
            database_=s.cat_database; day_=s.game_day;
            for(auto& cats:cats_) cats.clear();
        }
    }
    void Remember(const NewbornPlan& p,const NewbornDecision& d) {
        if(!Matches(p.source) || !d.discard || !d.cat.active || d.cat.record_only || ProtectedCat(d.cat)) return;
        cats_[int(p.kind)][d.cat.id]={d.choice,d.alternative_location,d.selection_label,d.npc};
    }
    int Prepare(NewbornPlan& p) {
        if(!p.Valid()) return 0;
        Observe(p.source);
        int restored=0;
        auto& cats=cats_[int(p.kind)];
        for(auto& d:p.decisions) if(d.discard && d.cat.active && !d.cat.record_only && !ProtectedCat(d.cat)) {
            const auto old=cats.find(d.cat.id);
            if(Matches(p.source) && old!=cats.end()) {
                d.choice=old->second.choice; d.alternative_location=old->second.location;
                d.npc=old->second.npc; d.selection_label=old->second.label;
                ++restored;
            } else {
                d.choice=ReviewChoice::Discard; d.npc=-1; d.alternative_location.clear(); d.selection_label.clear();
                for(const auto& npc:CatNpcChoices(p.source,d.cat.id)) if(npc.npc>=0 && npc.npc<8 && npc.enabled) {
                    d.choice=ReviewChoice::Npc; d.npc=npc.npc; d.selection_label=npc.label; break;
                }
                Remember(p,d);
            }
        }
        return restored;
    }
};
}
