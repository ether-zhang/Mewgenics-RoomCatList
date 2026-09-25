#pragma once
namespace roomcats {
enum CatColumn { Position, SendTo, Name, Parents, Age, SexOrientation, Total,
    Strength, Dexterity, Constitution, Intelligence, Speed, Charisma, Luck,
    GoodMutations, BadMutations, Diseases, CatColumnCount };
inline constexpr const char* kCatColumnLabels[] = {"位置", "送至", "名字/职业", "父母", "年龄", "性/取/繁", "总",
    "力", "敏", "体", "智", "速", "魅", "运", "好突变", "坏突变", "疾病"};
inline constexpr float kCatColumnWidths[] = {170, 156, 192, 200, 96, 84, 68, 54, 54, 54, 54, 54, 54, 54, 108, 108, 420};
inline bool SortableCatColumn(int column) { return column == Age || (column >= Total && column <= Luck); }
}
