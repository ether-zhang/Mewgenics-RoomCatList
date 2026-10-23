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
    assert(!age.Click(Parents) && age.column == Age);
    assert(SortCatRows({}, age).empty());
    std::vector<Cat> mixed{cats[0], cats[1]};
    mixed[0].details.real = {10, 0, 0, 0, 0, 0, 0};
    mixed[1].details.real = {8, 2, 2, 2, 2, 2, 2};
    mixed[0].details.genetic = {1, 5, 5, 5, 5, 5, 5};
    mixed[1].details.genetic = {9, 0, 0, 0, 0, 0, 0};
    CatSort total;
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{1, 0}));
    cats[0].name="Alpha"; cats[1].name="Beta"; cats[2].name="Alpha"; cats[3].name="Delta";
    CatSort names; names.Click(Name);
    assert(names.Ascending() && (SortCatRows(cats,names)==std::vector<int>{0,2,1,3}));
    names.Click(Name); assert((SortCatRows(cats,names)==std::vector<int>{3,1,0,2}));
    for(int col:{GoodMutations,BadMutations,Diseases}) {
        cats[0].details.good.resize(1); cats[0].details.bad.resize(1); cats[0].details.diseases.resize(1);
        cats[1].details.good.resize(3); cats[1].details.bad.resize(3); cats[1].details.diseases.resize(3);
        CatSort count; count.Click(col);
        assert((SortCatRows(cats,count)==std::vector<int>{1,0,2,3}));
        count.Click(col); assert((SortCatRows(cats,count)==std::vector<int>{2,0,1,3}));
        count.Click(col); assert(count.mode==0);
    }
    cats[0].location_key="A"; cats[1].location_key="B"; cats[2].location_key="A"; cats[3].location_key="B";
    CatSort grouped; grouped.Click(Age);
    assert((SortCatRows(cats,grouped,CatGroup::Location)==std::vector<int>{2,0,1,3}));
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{0, 1}));
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{0, 1}));
    total.Click(Total);
    assert((SortCatRows(mixed, total) == std::vector<int>{1, 0}));
    std::cout << "Sorting passed: both age directions, all four modes on seven attributes and totals, stable ties and unavailable data last.\n";
}
