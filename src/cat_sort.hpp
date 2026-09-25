#pragma once
#include "game_reader.hpp"
#include "cat_columns.hpp"
#include <algorithm>
#include <numeric>

namespace roomcats {
struct CatSort {
    int column = -1;
    // Four explicit modes: real descending/ascending, genetic descending/ascending.
    int mode = 0;
    bool Click(int clicked_column) {
        if (!SortableCatColumn(clicked_column)) return false;
        mode = column == clicked_column ? (mode + 1) % (clicked_column == Age ? 2 : 4) : 0;
        column = clicked_column;
        return true;
    }
    bool Genetic() const { return mode >= 2; }
    bool Ascending() const { return (mode & 1) != 0; }
};

inline std::vector<int> SortCatRows(const std::vector<Cat>& cats, const CatSort& sort) {
    std::vector<int> order(cats.size());
    std::iota(order.begin(), order.end(), 0);
    if (!SortableCatColumn(sort.column)) return order;
    std::stable_sort(order.begin(), order.end(), [&](int ia, int ib) {
        const auto& a = cats[ia];
        const auto& b = cats[ib];
        if (a.details.valid != b.details.valid) return a.details.valid;
        if (a.details.valid) {
            const auto& a_stats = sort.Genetic() ? a.details.genetic : a.details.real;
            const auto& b_stats = sort.Genetic() ? b.details.genetic : b.details.real;
            const std::int64_t av = sort.column == Age ? a.details.age :
                sort.column == Total ? TotalStats(a_stats) : a_stats[sort.column - Strength];
            const std::int64_t bv = sort.column == Age ? b.details.age :
                sort.column == Total ? TotalStats(b_stats) : b_stats[sort.column - Strength];
            if (av != bv) return sort.Ascending() ? av < bv : av > bv;
        }
        return a.id < b.id; // Stable ties across refreshes, including duplicate names.
    });
    return order;
}
}
