#include "localization.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "game_reader.hpp"
#include <array>
#include <cstring>
#include <limits>
#include <unordered_set>
#include <intrin.h>
#include <cmath>
#include <algorithm>

namespace roomcats {
namespace {
constexpr std::uintptr_t kDirectorRva = 0x13DAC30;
constexpr std::size_t kMaxCats = 10000;
constexpr std::size_t kMaxString = 256;
NativeCatLoader g_parent_loader = nullptr;
NativeRoomEffectsReader g_room_effects_reader = nullptr;
std::uintptr_t g_parent_manager = 0;
int g_parent_day = -1;
std::unordered_set<std::uint64_t> g_parent_load_attempts;

template<class T> bool Read(std::uintptr_t address, T& value) noexcept {
    return ReadBytes(address, &value, sizeof(value));
}
std::uintptr_t Pointer(std::uintptr_t address) noexcept {
    std::uintptr_t result = 0;
    Read(address, result);
    return result;
}
bool ByteIsZero(std::uintptr_t address) noexcept {
    unsigned char b = 1;
    return Read(address, b) && b == 0;
}

// MSVC x64 basic_string: a 16-byte inline buffer, then size and capacity.
struct EngineString {
    unsigned char storage[16];
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(EngineString) == 32);

bool StringStorage(std::uintptr_t address, bool wide,
                   EngineString& s, std::uintptr_t& data) {
    if (!Read(address, s) || s.size > kMaxString ||
        s.capacity < s.size || s.capacity > 1024 * 1024) return false;
    const auto inline_capacity = wide ? 7ULL : 15ULL;
    if (s.capacity <= inline_capacity) {
        if (s.size > inline_capacity) return false;
        data = address;
    } else {
        std::memcpy(&data, s.storage, sizeof(data));
        if (s.size && data < 0x10000) return false;
    }
    return true;
}

bool IsType(std::uintptr_t component, std::uintptr_t image,
            const char* expected) {
    // Read RTTI, without invoking any function in the game or changing caches.
    const auto table = Pointer(component);
    if (table < image + 8 || table - image > 0x3000000) return false;
    const auto locator = Pointer(table - 8);
    if (locator < image || locator - image > 0x3000000) return false;
    std::array<std::uint32_t, 6> info{};
    if (!Read(locator, info) || info[0] != 1 || info[3] > 0x3000000 ||
        image + info[5] != locator) return false;
    const auto length = std::strlen(expected) + 1;
    std::array<char, 100> actual{};
    return length <= actual.size() &&
        ReadBytes(image + info[3] + 16, actual.data(), length) &&
        std::memcmp(actual.data(), expected, length) == 0;
}

struct ComponentList {
    std::uint32_t capacity;
    std::uint32_t size;
    std::uintptr_t data;
};

bool Components(std::uintptr_t scene, std::vector<std::uintptr_t>& result) {
    ComponentList list{};
    if (!Read(Pointer(scene + 0x18), list) || list.size > list.capacity ||
        list.size > 100000 || (list.size && !list.data)) return false;
    result.resize(list.size);
    return list.size == 0 || ReadBytes(list.data, result.data(), list.size * sizeof(std::uintptr_t));
}

std::uintptr_t CachedCatData(std::uintptr_t manager, std::uint64_t id) {
    // Read the already-populated CatData hash map. Never call the game's
    // lookup function: its cache-miss path can populate or mutate the cache.
    const auto head = Pointer(manager + 0xF0);
    const auto buckets = Pointer(manager + 0x100);
    std::uint64_t mask = 0;
    if (!Read(manager + 0x118, mask) || mask > 0x100000 ||
        !head || !buckets || ((mask + 1) & mask) != 0) return 0;
    const auto bucket = buckets + (HashCatId(id) & mask) * 16;
    const auto first = Pointer(bucket);
    auto node = Pointer(bucket + 8);
    for (std::size_t tries = 0; node && node != head && tries < kMaxCats; ++tries) {
        std::uint64_t key = 0;
        if (!Read(node + 0x10, key)) return 0;
        if (key == id) return Pointer(node + 0x18);
        if (node == first) break;
        const auto previous = Pointer(node + 8);
        if (previous == node) break;
        node = previous;
    }
    return 0;
}
}

bool ReadBytes(std::uintptr_t address, void* destination, std::size_t size) noexcept {
    if (!destination || address < 0x10000 || size > 1024 * 1024 ||
        address > std::numeric_limits<std::uintptr_t>::max() - size) return false;
    __try {
        std::memcpy(destination, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadNarrowString(std::uintptr_t address, std::string& result) {
    result.clear();
    EngineString s{};
    std::uintptr_t data = 0;
    if (!StringStorage(address, false, s, data)) return false;
    std::array<char, kMaxString> text{};
    if (s.size && !ReadBytes(data, text.data(), static_cast<std::size_t>(s.size))) return false;
    result.assign(text.data(), static_cast<std::size_t>(s.size));
    return result.find('\0') == std::string::npos;
}

bool ReadWideString(std::uintptr_t address, std::string& result) {
    result.clear();
    EngineString s{};
    std::uintptr_t data = 0;
    if (!StringStorage(address, true, s, data)) return false;
    if (s.size == 0) return true;
    std::array<wchar_t, kMaxString> text{};
    if (!ReadBytes(data, text.data(), static_cast<std::size_t>(s.size) * 2)) return false;
    const auto count = static_cast<int>(s.size);
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), count, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return false;
    result.resize(bytes);
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), count,
            result.data(), bytes, nullptr, nullptr) != bytes) return false;
    return result.find('\0') == std::string::npos;
}

std::uint64_t HashCatId(std::uint64_t value) noexcept {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (int i = 0; i < 8; ++i) {
        hash = (hash ^ (value & 0xFF)) * 0x100000001B3ULL;
        value >>= 8;
    }
    return hash;
}

bool ValidateGameBuild(std::uintptr_t image, std::string& reason) {
    // These instructions independently establish the director, focused room,
    // HouseCat room field, and CatData-cache layouts on Steam build 25143593.
    struct Signature { std::uint32_t rva; const char* bytes; std::size_t count; };
    const Signature checks[] = {
        {0xE1A4C, "\x48\x8B\x1D\xDD\x91\x2F\x01", 7},
        {0xEAC97, "\xE8\x94\x1F\x00\x00\x48\x8B\x88\x88\x00\x00\x00", 12},
        {0xEAD3E, "\x48\x85\xFF\x74\x0A\x49\x39\xBC\x24\xE8\x00\x00\x00", 13},
        {0xEB350, "\x48\x8B\x05\xD9\xF8\x2E\x01\x48\x8B\x92\x80\x00\x00\x00\x48\x8B\x88\x98\x05\x00\x00", 21},
        {0xD72DA, "\x4C\x8B\x8F\x18\x01\x00\x00\x4C\x23\xCA\x4C\x8B\x97\x00\x01\x00\x00", 17},
        {0xD732F, "\x48\x8B\x41\x18\x48\x8B\x5C\x24\x50", 9}
    };
    std::array<unsigned char, 64> buffer{};
    for (const auto& check : checks) {
        if (!ReadBytes(image + check.rva, buffer.data(), check.count) ||
            std::memcmp(buffer.data(), check.bytes, check.count) != 0) {
            reason = "Unsupported game build: memory-layout signature did not match at RVA " + std::to_string(check.rva);
            return false;
        }
    }
    reason.clear();
    return true;
}

std::uintptr_t FindHouseHudRenderer(std::uintptr_t image, std::uintptr_t scene) {
    std::vector<std::uintptr_t> components;
    if (!Components(scene, components)) return 0;
    for (const auto component : components) {
        if (!IsType(component, image, ".?AVHouse@glaiel@@")) continue;
        const auto renderer = Pointer(component + 0x48);
        std::string name;
        if (Pointer(renderer + 0x20) == scene &&
            ReadNarrowString(renderer + 0xA8, name) && name == "HouseStatusUI") return renderer;
    }
    return 0;
}

bool ComponentHasType(std::uintptr_t component, std::uintptr_t image, const char* type) {
    return IsType(component, image, type);
}

std::uintptr_t FindCachedCatData(std::uintptr_t image, std::uint64_t id) {
    return CachedCatData(Pointer(Pointer(image + kDirectorRva) + 0x598), id);
}

void ConfigureRoomEffectsReader(NativeRoomEffectsReader reader) { g_room_effects_reader=reader; }

bool ReadComfortEffect(std::uintptr_t vector, double& comfort) {
    comfort=0;
    std::array<std::uintptr_t,3> range{};
    if(!Read(vector,range) || range[1]<range[0] || range[2]<range[1] ||
        (range[1]-range[0])%40 || range[2]-range[0]>40*4096) return false;
    for(auto at=range[0];at<range[1];at+=40) {
        std::string name; double value=0;
        if(!ReadNarrowString(at,name) || !Read(at+32,value) || !std::isfinite(value) || std::abs(value)>100000) return false;
        if(name=="Comfort") comfort+=value;
    }
    return std::isfinite(comfort) && std::abs(comfort)<=100000;
}

bool ReadPopulationComfort(RoomSnapshot& s) {
    s.population_comfort_valid=false;
    if(!s.valid || !s.in_house || s.suspended || !g_room_effects_reader) return false;
    for(auto& c:s.cats) if(c.active && !c.record_only)
        if(!ReadComfortEffect(c.component+0xA0,c.comfort_effect)) return false;
    for(auto& room:s.locations) if(room.kind==LocationKind::Room) {
        // The native getter refreshes dirty effects and returns a borrowed vector.
        const auto effects=g_room_effects_reader(room.component);
        double total=0; std::uint32_t count=0;
        if(effects!=room.component+0x140 || !ReadComfortEffect(effects,total) ||
            !Read(room.component+0x6C,count) || count>kMaxCats) return false;
        std::vector<std::uintptr_t> members(count), expected;
        if(count && !ReadBytes(Pointer(room.component+0x70),members.data(),count*sizeof(std::uintptr_t))) return false;
        for(const auto& c:s.cats) if(c.active && !c.record_only && c.location==room.component) {
            expected.push_back(c.component); total-=c.comfort_effect;
        }
        std::sort(members.begin(),members.end()); std::sort(expected.begin(),expected.end());
        if(members!=expected || !std::isfinite(total) || std::abs(total)>100000) return false;
        room.comfort_base=total;
    }
    s.population_comfort_valid=true; return true;
}

void ConfigureParentCatLoader(NativeCatLoader loader) {
    g_parent_loader = loader;
    g_parent_manager = 0; g_parent_day = -1;
    g_parent_load_attempts.clear();
}

bool ReadParentIds(std::uintptr_t pedigree, std::uint64_t cat, std::array<std::uint64_t, 2>& parents) {
    parents = {};
    // Game Pedigree::child_to_parents: phmap flat hash table, 32-byte slots.
    struct Header { std::uintptr_t ctrl, slots; std::uint64_t size, capacity; } h{};
    if (!cat || cat > INT64_MAX || !Read(pedigree, h) || !h.ctrl || !h.slots || !h.size ||
        h.size > h.capacity || h.capacity > 0xFFFFF || ((h.capacity + 1) & h.capacity)) return false;
    std::uint64_t high = 0;
    const auto low = _umul128(cat, 0xDE5FB9D2630458E9ULL, &high);
    const auto hash = low + high;
    const unsigned char fingerprint = hash & 0x7F;
    auto offset = (hash >> 7) & h.capacity;
    std::uint64_t stride = 0;
    for (std::uint64_t group = 0; group <= h.capacity / 16 + 1; ++group) {
        std::array<unsigned char, 16> controls{};
        if (!Read(h.ctrl + offset, controls)) return false;
        bool empty = false;
        for (unsigned i = 0; i < 16; ++i) {
            empty |= controls[i] == 0x80;
            const auto slot = (offset + i) & h.capacity;
            if (controls[i] != fingerprint || slot >= h.capacity) continue;
            struct Record { std::uint64_t child; std::int64_t a, b; double coefficient; } record{};
            if (!Read(h.slots + slot*32, record)) return false;
            if (record.child != cat) continue;
            if (record.a < -1 || record.b < -1 || record.a == cat || record.b == cat ||
                !std::isfinite(record.coefficient) || record.coefficient < 0 || record.coefficient > 1) return false;
            parents = {record.a > 0 ? std::uint64_t(record.a) : 0, record.b > 0 ? std::uint64_t(record.b) : 0};
            return true;
        }
        if (empty) return false;
        stride += 16;
        offset = (offset + stride) & h.capacity;
    }
    return false;
}

namespace {
std::uintptr_t LoadParentData(std::uintptr_t manager, std::uint64_t id, int game_day) {
    if (g_parent_manager != manager || g_parent_day != game_day) {
        g_parent_manager = manager; g_parent_day = game_day;
        g_parent_load_attempts.clear();
    }
    auto data = CachedCatData(manager,id);
    if (!data && g_parent_loader && g_parent_load_attempts.size() < kMaxCats*2 && g_parent_load_attempts.insert(id).second)
        data = g_parent_loader(manager,id);
    return data && Pointer(data+0xC48) == id ? data : 0;
}
}

CatFamily ReadCatFamily(std::uintptr_t manager, std::uint64_t cat, int game_day) {
    CatFamily family;
    if (!ReadParentIds(manager + 0x38, cat, family.ids)) return family;
    family.valid = true;
    for (int i = 0; i < 2; ++i) {
        const auto id = family.ids[i];
        if (!id) continue;
        // Historical parents may be absent from the active-cat cache. Use
        // the game's ordinary CatDatabase getter at most once per parent/day.
        // It may load its own cache; this mod never opens or edits save files.
        const auto data = LoadParentData(manager,id,game_day);
        if (!data || !ReadWideString(data + 0x18, family.names[i])) continue;
        if (family.names[i].empty()) family.names[i] = roomcats::Tx("未命名猫咪");
        for (auto& ch : family.names[i]) if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
    }
    return family;
}

RoomSnapshot ReadFocusedRoom(std::uintptr_t image, bool include_cats, bool all_cats,
                            const std::array<std::uint64_t,2>* requested_ids) {
    RoomSnapshot out;
    const auto director = Pointer(image + kDirectorRva);
    Read(director+0x580,out.game_day);
    out.cat_database=Pointer(director+0x598);
    const auto root = Pointer(director + 0x28);
    const auto begin = Pointer(root);
    const auto end = Pointer(root + 8);
    if (!begin || end < begin || end - begin > 256 * 8 || (end - begin) % 8) return out;
    for (auto p = begin; p < end; p += 8) {
        const auto candidate = Pointer(p);
        std::string name;
        if (ReadNarrowString(candidate + 0x4B8, name) && name == "House") {
            out.scene = candidate;
            break;
        }
    }
    if (!out.scene || !ByteIsZero(out.scene + 0x4B0)) return out;
    Read(out.scene - 8, out.generation);
    if (!ByteIsZero(out.scene + 0x4D8) || !ByteIsZero(out.scene + 0x4DA) || !ByteIsZero(out.scene + 0x4DB)) {
        out.suspended = true;
        return out;
    }
    std::vector<std::uintptr_t> components;
    if (!Components(out.scene, components)) return out;
    std::uintptr_t house = 0;
    for (const auto component : components) {
        if (IsType(component, image, ".?AVHouse@glaiel@@")) {
            if (house) return out; // An ambiguous scene is not a usable snapshot.
            house = component;
        }
    }
    if (!house) return out;
    out.in_house = true;
    Read(out.scene - 8, out.generation);
    for (const auto component : components) {
        if (Pointer(component + 0x20) != out.scene) continue;
        if (IsType(component, image, ".?AVCatStatsDrawer@glaiel@@")) out.cat_drawer = component;
        else if (IsType(component, image, ".?AVNPCMapDrawer@glaiel@@")) out.npc_drawer = component;
        else if (IsType(component, image, ".?AVHouseDrawerUI@glaiel@@")) out.drawer_ui = component;
        else if (IsType(component, image, ".?AVHousePipe@glaiel@@")) out.pipe = component;
    }
    out.room = requested_ids ? 0 : Pointer(house + 0x88);
    if (out.room && (!IsType(out.room, image, ".?AVFurnitureGrid@glaiel@@") ||
        Pointer(out.room + 0x20) != out.scene ||
        !ReadNarrowString(out.room + 0x40, out.room_id) || out.room_id.empty())) {
        out.error = roomcats::Tx("房间数据暂时不可用，请稍后重试。");
        return out;
    }
    if (!include_cats) {
        out.valid = true;
        return out;
    }
    unsigned boxes = 0;
    for (const auto component : components) {
        if (Pointer(component + 0x20) != out.scene) continue;
        CatLocation location;
        location.component = component;
        Read(component - 8, location.generation);
        if (IsType(component, image, ".?AVFurnitureGrid@glaiel@@")) {
            std::string id;
            if (!ReadNarrowString(component + 0x40, id) || id.empty()) continue;
            location.key = "room:" + id;
            location.label = RoomLabel(id);
        } else if (IsType(component, image, ".?AVButchBox@glaiel@@")) {
            location.kind = LocationKind::Box;
            location.key = "box:" + std::to_string(component);
            location.label = ++boxes == 1 ? roomcats::Tx("箱子") : roomcats::Tx("箱子 ") + std::to_string(boxes);
        } else continue;
        out.locations.push_back(std::move(location));
    }
    const auto manager = Pointer(director + 0x598);
    static std::uintptr_t detail_scene = 0;
    static std::uint64_t detail_generation = 0;
    if (detail_scene != out.scene || detail_generation != out.generation) {
        ClearCatDetailsCache();
        g_parent_load_attempts.clear();
        detail_scene = out.scene; detail_generation = out.generation;
    }
    int game_day = -1;
    Read(director + 0x580, game_day);
    std::unordered_set<std::uint64_t> seen;
    for (const auto component : components) {
        if (!IsType(component, image, ".?AVHouseCat@glaiel@@") ||
            (!all_cats && out.room && Pointer(component + 0xE8) != out.room)) continue;
        Cat cat;
        if (requested_ids) {
            std::uint8_t live = 0;
            const auto id = Pointer(component+0x80);
            if (std::find(requested_ids->begin(),requested_ids->end(),id) == requested_ids->end() ||
                !Read(component+0x88,live) || !live || Pointer(component+0x20) != out.scene) continue;
        }
        cat.component = component;
        std::uint8_t active = 0;
        cat.active = Read(component+0x88,active) && active != 0;
        // A native disposal can remove CatData one tick before its inactive
        // HouseCat leaves the scene array. Batch/action snapshots exclude it.
        if(all_cats && !cat.active) continue;
        Read(component - 8, cat.generation);
        cat.location = Pointer(component + 0xE8);
        cat.location_label = cat.location ? roomcats::Tx("其他位置") : roomcats::Tx("未分配");
        for (const auto& location : out.locations)
            if (location.component == cat.location) {
                cat.location_key = location.key;
                cat.location_label = location.label;
                break;
            }
        if (!Read(component + 0x80, cat.id) || cat.id == 0 ||
            seen.count(cat.id) || out.cats.size() >= kMaxCats) {
            out.error = roomcats::Tx("猫咪数据正在变化，请重新打开列表。");
            out.cats.clear();
            return out;
        }
        seen.insert(cat.id);
        const auto data = CachedCatData(manager, cat.id);
        if (!data || !ReadWideString(data + 0x18, cat.name)) {
            out.error = roomcats::Tx("部分猫咪的名字未能读取，请稍后重试。");
            out.cats.clear();
            return out;
        }
        if (cat.name.empty()) cat.name = roomcats::Tx("未命名猫咪");
        for (auto& c : cat.name) if (c == '\r' || c == '\n' || c == '\t') c = ' ';
        cat.details = ReadCatDetails(data, cat.id, game_day);
        cat.parents = ReadCatFamily(manager, cat.id, game_day);
        out.cats.push_back(std::move(cat));
    }
    out.valid = true;
    return out;
}

RoomSnapshot ReadParentCats(std::uintptr_t image, const std::array<std::uint64_t,2>& ids,
                           const std::array<std::string,2>& known_names) {
    // Only hydrate the requested actors; the room/box destinations and native
    // action context are identical to the main list, regardless of room focus.
    auto out = ReadFocusedRoom(image,true,true,&ids);
    if (!out.valid || !out.in_house) return out;
    const auto director = Pointer(image+kDirectorRva);
    const auto manager = Pointer(director+0x598);
    int day = -1; Read(director+0x580,day);
    auto live = std::move(out.cats);
    out.cats.clear();
    for (int i=0;i<2;++i) {
        const auto id = ids[i];
        if (!id || (i && id == ids[0])) continue;
        const auto found = std::find_if(live.begin(),live.end(),[id](const Cat& c) { return c.id == id; });
        if (found != live.end()) { out.cats.push_back(*found); continue; }
        Cat cat;
        cat.id = id; cat.record_only = true; cat.name = known_names[i];
        const auto data = LoadParentData(manager,id,day);
        cat.location_label = data ? roomcats::Tx("不在家园") : roomcats::Tx("资料未找到");
        if (data) {
            std::string name;
            if (ReadWideString(data+0x18,name) && !name.empty()) cat.name = std::move(name);
            cat.details = ReadCatDetails(data,id,day);
            std::int64_t death = -1;
            if (Read(data+0xC40,death) && death >= 0) cat.location_label = roomcats::Tx("已故");
        }
        cat.parents = ReadCatFamily(manager,id,day);
        if (cat.name.empty()) cat.name = roomcats::Tx("未知父母");
        for (auto& ch : cat.name) if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
        out.cats.push_back(std::move(cat));
    }
    return out;
}
}
