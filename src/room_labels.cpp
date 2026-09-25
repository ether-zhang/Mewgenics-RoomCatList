#include "localization.hpp"
#include "game_reader.hpp"
#include <utility>
namespace roomcats {
std::string RoomLabel(const std::string& id) {
    if (id.empty()) return roomcats::Tx("全屋猫咪");
    if (id == "Attic" || id == "SmallAttic") return roomcats::Tx("阁楼");
    if (id == "Floor1_Large") return roomcats::Tx("一楼左");
    if (id == "Floor1_Small") return roomcats::Tx("一楼右");
    if (id == "Floor2_Large") return roomcats::Tx("二楼右");
    if (id == "Floor2_Small") return roomcats::Tx("二楼左");
    if (id == "AdventureBox") return roomcats::Tx("出征区域");
    if (id.size() == 9 && id.compare(0, 8, "Basement") == 0 && id[8] >= '0' && id[8] <= '4')
        return roomcats::Tx("地下室 ") + std::to_string(id[8] - '0' + 1);
    return id;
}
std::string LocalizedLocationLabel(const std::string& key, const std::string& old_label) {
    if (key.rfind("room:", 0) == 0) return RoomLabel(key.substr(5));
    if (key.rfind("box:", 0) == 0) {
        const auto prefix = old_label.rfind("箱子", 0) == 0 ? std::string("箱子") :
            old_label.rfind("Box", 0) == 0 ? std::string("Box") : std::string{};
        return std::string(Tx("箱子")) + (prefix.empty() ? std::string{} : old_label.substr(prefix.size()));
    }
    for (const auto& names : {std::pair{"其他位置","Other location"},
            std::pair{"未分配","Unassigned"},std::pair{"不在家园","Not at home"},
            std::pair{"资料未找到","Record not found"},std::pair{"已故","Deceased"}})
        if (old_label == names.first || old_label == names.second) return Tx(names.first);
    return old_label;
}
}
