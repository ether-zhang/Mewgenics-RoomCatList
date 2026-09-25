#include "../src/native_args.hpp"
#include <cassert>
#include <string>
#include <vector>

void TestNativeArgs() {
    using Strings = std::vector<std::string>;
    const std::string folder = "F:/Game Folder/mods/RoomCatList";
    const std::string base = "F:/Game Folder";
    assert((roomcats::AddOwnModPath({"Mewgenics.exe"}, folder, base) ==
        Strings{"Mewgenics.exe", "-modpaths", folder}));
    const Strings existing{"Mewgenics.exe", "-modpaths", "OtherOne", "OtherTwo", "-fullscreen", "0"};
    assert((roomcats::AddOwnModPath(existing, folder, base) ==
        Strings{"Mewgenics.exe", "-modpaths", folder, "OtherOne", "OtherTwo", "-fullscreen", "0"}));
    const Strings relative{"Mewgenics.exe", "-modpaths", ".\\mods\\RoomCatList", "-devmode"};
    assert(roomcats::AddOwnModPath(relative, folder, base) == relative);
    const Strings mixed_case{"Mewgenics.exe", "-MODPATHS", "f:\\game folder\\mods\\ROOMCATLIST\\"};
    assert(roomcats::AddOwnModPath(mixed_case, folder, base) == mixed_case);
    const Strings similar{"Mewgenics.exe", "-modpaths", "mods/RoomCatListOther"};
    assert(roomcats::AddOwnModPath(similar, folder, base).size() == similar.size() + 1);
    const Strings elsewhere{"Mewgenics.exe", "-output", folder};
    assert(roomcats::AddOwnModPath(elsewhere, folder, base).size() == elsewhere.size() + 2);
    const std::string unicode_folder = "F:/中文 目录/mods/RoomCatList";
    assert(roomcats::NormalizeModPath(".\\mods\\RoomCatList", "F:/中文 目录") ==
           roomcats::NormalizeModPath(unicode_folder));
    assert(roomcats::AddOwnModPath({}, folder).empty());
}
