#include "../src/cat_sort.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace roomcats;
    std::vector<Cat> cats(4);
    for (int i = 0; i < 4; ++i) { cats[i].id = i + 1; cats[i].name = "same"; cats[i].details.valid = i != 3; }
    cats[0].details.real.fill(9); cats[0].details.genetic.fill(2); cats[0].details.age = 3;
    cats[1].details.real.fill(3); cats[1].details.genetic.fill(8); cats[1].details.age = 7;
    cats[2].details.real.fill(9); cats[2].details.genetic.fill(5); cats[2].details.age = 7;
    for (int column = Total; column <= Luck; ++column) {
        CatSort sort;
        assert(sort.Click(column));
        assert((SortCatRows(cats, sort) == std::vector<int>{0, 2, 1, 3}));
        sort.Click(column);
        assert((SortCatRows(cats, sort) == std::vector<int>{1, 0, 2, 3}));
        sort.Click(column);
        assert((SortCatRows(cats, sort) == std::vector<int>{1, 2, 0, 3}));
        sort.Click(column);
        assert((SortCatRows(cats, sort) == std::vector<int>{0, 2, 1, 3}));
        sort.Click(column);
        assert(sort.mode == 0);
    }
    CatSort age;
    age.Click(Age);
    assert((SortCatRows(cats, age) == std::vector<int>{1, 2, 0, 3}));
    age.Click(Age);
    assert((SortCatRows(cats, age) == std::vector<int>{0, 1, 2, 3}));
    assert(!age.Click(SexOrientation) && age.column == Age);
    assert(SortCatRows({}, age).empty());
    std::vector<Cat> mixed{cats[0], cats[1]};
    mixed[0].details.real = {10, 0, 0, 0, 0, 0, 0};
    mixed[1].details.real = {8, 2, 2, 2, 2, 2, 2};
    mixed[0].details.genetic = {1, 5, 5, 5, 5, 5, 5};
    mixed[1].details.genetic = {9, 0, 0, 0, 0, 0, 0};
    CatSort total;
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{1, 0}));
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{0, 1}));
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{0, 1}));
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{1, 0}));
    std::cout << "Sorting passed: both age directions, all four modes on seven attributes and totals, stable ties and unavailable data last.\n";
}
