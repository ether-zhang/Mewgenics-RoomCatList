#pragma once
#include "localization.hpp"
#include <array>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>

namespace roomcats {
struct BreedingRules {
    std::array<int,3> genetic_min{48,46,45};
    bool prioritize_dex=true;
    int preferred_dex=7;
    bool reject_bad=true, reject_disease=true;
};
struct BattleRules {
    int real_min=52;
    bool reject_lopsided=true;
    int low_stat_max=5, low_stat_count=2;
};
struct NewbornRules {
    bool demote_lowest_parent=true;
    BreedingRules breeding;
    BattleRules battle;
    int attic_limit=2, second_limit=3, second_cross_target=1;
    bool promote=true;
    int promotion_limit=2;
    bool split_parents=true, mark_birth=true, class_parent_no_battle=true;
};
struct AdultRules {
    BreedingRules breeding;
    BattleRules battle;
    int population_target=90;
    std::array<int,4> comfort_min{15,5,5,0};
    bool allow_mild_first=true, class_no_battle=true;
};
struct ScreeningRules { NewbornRules newborn; AdultRules adult; };

inline const char* ScreeningRuleHelp(const char* key) {
    const std::string_view name=key;
    if(name.find("genetic_")==0) return Tx("七项遗传属性相加后的最低值，不是单项属性，也不是真实属性。达到要求后仍要比较名额和其他猫的品质。");
    if(name=="prioritize_dex" || name=="preferred_dex") return Tx("顶楼先保留遗传敏捷等于指定值的猫，再按遗传总属性、优质突变数量和职业比较。");
    if(name=="reject_bad" || name=="reject_disease") return Tx("符合条件的猫不再参加繁育房分配，改按战备标准判断；达标的留在一楼右，未达标的进入待处理名单。大块头和侏儒症不按疾病处理。");
    if(name=="real_min") return Tx("七项真实属性相加后的最低值。低于此值的战备猫进入待处理名单，等待你选择投送、遗弃或保留。");
    if(name=="reject_lopsided" || name=="low_stat_max" || name=="low_stat_count") return Tx("例如：单项不高于5、至少2项，则两项或更多真实属性小于等于5的猫会进入待处理名单。");
    if(name=="second_cross_target") return Tx("二楼左、右互相分配新生猫时，优先达到这个数量；它包含在每房的新生保留上限内，不是额外名额。");
    if(name=="promote" || name=="promotion_limit") return Tx("达到上一级繁育要求的新生猫可上调一级。数量按本轮开始时所在楼层统计；仍受目标房间的保留上限约束。");
    if(name=="split_parents" || name=="demote_lowest_parent") return Tx("近亲小猫的父母仍同住繁育房时，将遗传品质较弱的一方下调一级。一楼左的下一站是一楼右战备房；房子标记的猫受保护。");
    if(name=="class_no_battle" || name=="class_parent_no_battle") return Tx("带职业大猫若未能留在繁育房，就进入待处理名单，不转去战备房；原本在战备房的猫仍按战备规则筛选。");
    if(name=="population_target") return Tx("目标是全屋猫咪总数，但这里只调整年龄大于1的猫。幼猫、箱子里的猫和房子标记的猫计入总数；受保护限制时，结果可能高于目标。");
    if(name.find("comfort_")==0) return Tx("分配完成后，该繁育房希望维持的最低舒适度；猫咪和家具共同影响结果。受保护猫可能使目标无法达到。");
    return Tx("筛选先生成可复查的方案，不会立即改变猫咪；点击“确定处理”后才执行。");
}

// One field catalog drives editing, bounds, persistence and rule fingerprints.
// The second-floor limit bounds the existing exhaustive birth-room assignment
// to at most 12 candidates (4096 assignments), independent of house size.
template<class B,class F> void VisitBreedingRules(B& b,F&& f) {
    f("genetic_attic",roomcats::Tx("顶楼：遗传总属性至少"),b.genetic_min[0],0,700);
    f("genetic_second",roomcats::Tx("二楼：遗传总属性至少"),b.genetic_min[1],0,700);
    f("genetic_first",roomcats::Tx("一楼左：遗传总属性至少"),b.genetic_min[2],0,700);
    f("prioritize_dex",roomcats::Tx("顶楼优先保留指定遗传敏捷的猫"),b.prioritize_dex,0,1);
    f("preferred_dex",roomcats::Tx("顶楼优先的遗传敏捷值"),b.preferred_dex,0,100);
    f("reject_bad",roomcats::Tx("坏突变猫按战备标准筛选"),b.reject_bad,0,1);
    f("reject_disease",roomcats::Tx("患病猫按战备标准筛选"),b.reject_disease,0,1);
}
template<class B,class F> void VisitBattleRules(B& b,F&& f) {
    f("real_min",roomcats::Tx("战备猫真实总属性至少"),b.real_min,0,7000);
    f("reject_lopsided",roomcats::Tx("筛除多项真实属性偏低的猫"),b.reject_lopsided,0,1);
    f("low_stat_max",roomcats::Tx("单项真实属性不高于"),b.low_stat_max,-100,1000);
    f("low_stat_count",roomcats::Tx("至少几项偏低时筛除"),b.low_stat_count,1,7);
}
template<class R,class F> void VisitNewbornRules(R& r,F&& f) {
    VisitBreedingRules(r.breeding,f); VisitBattleRules(r.battle,f);
    f("attic_limit",roomcats::Tx("顶楼最多保留几只新生猫"),r.attic_limit,0,100);
    f("second_limit",roomcats::Tx("二楼每房最多保留几只新生猫"),r.second_limit,0,6);
    f("second_cross_target",roomcats::Tx("二楼每房优先保留几只对面出生的猫"),r.second_cross_target,0,6);
    f("promote",roomcats::Tx("允许达标新生猫调到上一级房间"),r.promote,0,1);
    f("promotion_limit",roomcats::Tx("每个起始楼层最多上调几只猫"),r.promotion_limit,0,100);
    f("split_parents",roomcats::Tx("将近亲小猫较弱的一方亲本调走"),r.split_parents,0,1);
    f("demote_lowest_parent",roomcats::Tx("一楼左的较弱亲本也调离繁育房"),r.demote_lowest_parent,0,1);
    f("mark_birth",roomcats::Tx("自动添加出生房间标记"),r.mark_birth,0,1);
    f("class_parent_no_battle",roomcats::Tx("落选的带职业亲本不留作战备"),r.class_parent_no_battle,0,1);
}
template<class R,class F> void VisitAdultRules(R& r,F&& f) {
    VisitBreedingRules(r.breeding,f); VisitBattleRules(r.battle,f);
    f("population_target",roomcats::Tx("家园总猫数目标"),r.population_target,1,10000);
    f("comfort_attic",roomcats::Tx("顶楼舒适度下限"),r.comfort_min[0],-1000,1000);
    f("comfort_second_left",roomcats::Tx("二楼左舒适度下限"),r.comfort_min[1],-1000,1000);
    f("comfort_second_right",roomcats::Tx("二楼右舒适度下限"),r.comfort_min[2],-1000,1000);
    f("comfort_first_left",roomcats::Tx("一楼左舒适度下限"),r.comfort_min[3],-1000,1000);
    f("allow_mild_first",roomcats::Tx("一楼左允许轻度近亲"),r.allow_mild_first,0,1);
    f("class_no_battle",roomcats::Tx("繁育落选的带职业大猫不留作战备"),r.class_no_battle,0,1);
}
template<class R,class F> void VisitScreeningRules(R& r,F&& f) {
    VisitNewbornRules(r.newborn,[&](const char* key,const char* label,auto& value,int lo,int hi) { f(std::string("newborn.")+key,label,value,lo,hi); });
    VisitAdultRules(r.adult,[&](const char* key,const char* label,auto& value,int lo,int hi) { f(std::string("adult.")+key,label,value,lo,hi); });
}
inline std::string ValidateScreeningRules(const ScreeningRules& rules) {
    std::string error;
    VisitScreeningRules(rules,[&](const auto&,const char* label,const auto& v,int lo,int hi) {
        if(error.empty() && (int(v)<lo || int(v)>hi)) error=std::string(label)+roomcats::Tx(" 需要在 ")+std::to_string(lo)+roomcats::Tx(" 至 ")+std::to_string(hi)+roomcats::Tx(" 之间");
    });
    if(!error.empty()) return error;
    for(const auto* b:{&rules.newborn.breeding,&rules.adult.breeding})
        if(b->genetic_min[0]<b->genetic_min[1] || b->genetic_min[1]<b->genetic_min[2]) return roomcats::Tx("遗传门槛需满足：顶楼 ≥ 二楼 ≥ 一楼左");
    if(rules.newborn.second_cross_target>rules.newborn.second_limit) return roomcats::Tx("对面出生数量不能超过二楼每房保留上限");
    return {};
}
inline std::string SerializeScreeningRules(const ScreeningRules& rules) {
    std::string out;
    VisitScreeningRules(rules,[&](const auto& key,const char*,const auto& value,int,int) { out+=key+"="+std::to_string(int(value))+"\n"; });
    return out;
}
inline bool ParseScreeningRules(const std::string& text,ScreeningRules& rules,std::string& error) {
    ScreeningRules parsed; std::istringstream input(text); std::string line;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.empty()) continue;
        const auto equals=line.find('='); if(equals==std::string::npos) continue;
        const auto key=line.substr(0,equals); const auto value=line.substr(equals+1);
        bool bad=false;
        VisitScreeningRules(parsed,[&](const auto& name,const char*,auto& field,int lo,int hi) {
            if(name!=key) return;
            int n=0; const auto result=std::from_chars(value.data(),value.data()+value.size(),n);
            if(result.ec!=std::errc{} || result.ptr!=value.data()+value.size() || n<lo || n>hi) { bad=true; return; }
            field=static_cast<std::decay_t<decltype(field)>>(n);
        });
        if(bad) { error=roomcats::Tx("配置项 ")+key+roomcats::Tx(" 的数值无效"); return false; }
    }
    error=ValidateScreeningRules(parsed);
    if(!error.empty()) return false;
    rules=parsed; return true;
}
template<class R> std::uint64_t RuleFingerprint(const R& rules) {
    std::uint64_t hash=14695981039346656037ULL;
    auto visit=[&](const char*,const char*,const auto& value,int,int) {
        const auto n=std::uint32_t(int(value));
        for(int i=0;i<4;++i) { hash^=(n>>(8*i))&255; hash*=1099511628211ULL; }
    };
    if constexpr(std::is_same_v<R,NewbornRules>) VisitNewbornRules(rules,visit);
    else VisitAdultRules(rules,visit);
    return hash;
}
}
