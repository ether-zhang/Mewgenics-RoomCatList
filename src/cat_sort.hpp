#pragma once
#include "game_reader.hpp"
#include "cat_columns.hpp"
#include <algorithm>
#include <numeric>

namespace roomcats {
enum class CatGroup { None, Location, Class, Marker };
inline int CatLocationOrder(const std::string& key) {
    static const char* rooms[]={"room:Attic","room:Floor2_Small","room:Floor2_Large","room:Floor1_Large","room:Floor1_Small"};
    for(int i=0;i<5;++i) if(key==rooms[i]) return i;
    return key.rfind("box:",0)==0 ? 5 : key.empty() ? 7 : 6;
}
inline std::string CatGroupKey(const Cat& cat, CatGroup group) {
    if (group == CatGroup::Location) return cat.location_key;
    if (group == CatGroup::Class) return cat.details.collar.empty() ? "Colorless" : cat.details.collar;
    if (group == CatGroup::Marker) return cat.details.marker;
    return {};
}
struct CatSort {
    int column = -1;
    // Four explicit modes: real descending/ascending, genetic descending/ascending.
    int mode = 0;
    bool Click(int clicked_column) {
        if (!SortableCatColumn(clicked_column)) return false;
        mode = column == clicked_column ? (mode + 1) % (StatCatColumn(clicked_column) ? 4 : 2) :
            (clicked_column == Name || clicked_column == Position ? 1 : 0);
        column = clicked_column;
        return true;
    }
    bool Genetic() const { return mode >= 2; }
    bool Ascending() const { return (mode & 1) != 0; }
};

inline std::vector<int> SortCatRows(const std::vector<Cat>& cats, const CatSort& sort, CatGroup group = CatGroup::None) {
    std::vector<int> order(cats.size());
    std::iota(order.begin(), order.end(), 0);
    if (!SortableCatColumn(sort.column) && group == CatGroup::None) return order;
    std::stable_sort(order.begin(), order.end(), [&](int ia, int ib) {
        const auto& a = cats[ia];
        const auto& b = cats[ib];
        if (group != CatGroup::None) {
            const auto ak = CatGroupKey(a,group), bk = CatGroupKey(b,group);
            if(group==CatGroup::Location && CatLocationOrder(ak)!=CatLocationOrder(bk))
                return CatLocationOrder(ak)<CatLocationOrder(bk);
            if (ak != bk) return ak < bk;
        }
        if (!SortableCatColumn(sort.column)) return a.id < b.id;
        if (sort.column == Name || sort.column == Position) {
            const auto& av = sort.column == Name ? a.name : a.location_key;
            const auto& bv = sort.column == Name ? b.name : b.location_key;
            if(sort.column==Position && CatLocationOrder(av)!=CatLocationOrder(bv))
                return sort.Ascending() ? CatLocationOrder(av)<CatLocationOrder(bv) : CatLocationOrder(av)>CatLocationOrder(bv);
            if (av != bv) return sort.Ascending() ? av < bv : av > bv;
            return a.id < b.id;
        }
        if (a.details.valid != b.details.valid) return a.details.valid;
        if (a.details.valid) {
            const auto& a_stats = sort.Genetic() ? a.details.genetic : a.details.real;
            const auto& b_stats = sort.Genetic() ? b.details.genetic : b.details.real;
            auto value = [&](const CatDetails& d, const Stats& stats)->std::int64_t {
                if (sort.column == Age) return d.age;
                if (sort.column == SexOrientation) return d.sex;
                if (sort.column == GoodMutations) return d.good.size();
                if (sort.column == BadMutations) return d.bad.size();
                if (sort.column == Diseases) return d.diseases.size();
                return sort.column == Total ? TotalStats(stats) : stats[sort.column - Strength];
            };
            const auto av = value(a.details,a_stats), bv = value(b.details,b_stats);
            if (av != bv) return sort.Ascending() ? av < bv : av > bv;
        }
        return a.id < b.id; // Stable ties across refreshes, including duplicate names.
    });
    return order;
}
}
