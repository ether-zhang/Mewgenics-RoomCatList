#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "cat_details.hpp"

namespace roomcats {
enum class LocationKind { Room, Box };
struct CatLocation {
    std::string key;
    std::string label;
    std::uintptr_t component = 0;
    std::uint64_t generation = 0;
    LocationKind kind = LocationKind::Room;
    double comfort_base = 0; // Furniture/world effects, excluding residents and crowding.
};
struct CatFamily {
    bool valid = false;
    std::array<std::uint64_t, 2> ids{};
    std::array<std::string, 2> names{};
};
struct Cat {
    std::uint64_t id = 0;
    std::string name;
    CatDetails details;
    std::uintptr_t component = 0;
    std::uintptr_t location = 0;
    std::string location_key;
    std::string location_label;
    std::uint64_t generation = 0;
    CatFamily parents;
    bool record_only = false; // Historical data has no usable HouseCat actor.
    bool active = true;
    double comfort_effect = 0; // Native HouseCat room effect, independent of its stats.
};

struct RoomSnapshot {
    bool in_house = false;
    bool valid = false;
    std::uintptr_t scene = 0;
    std::uintptr_t room = 0;
    std::string room_id;
    std::vector<Cat> cats;
    std::string error;
    std::uint64_t generation = 0;
    std::vector<CatLocation> locations;
    std::uintptr_t cat_drawer = 0, npc_drawer = 0, drawer_ui = 0, pipe = 0;
    bool suspended = false;
    int game_day = -1;
    bool population_comfort_valid = false;
    std::uintptr_t cat_database = 0; // Save-session identity, stable across NPC scenes.
};

bool ReadBytes(std::uintptr_t address, void* destination, std::size_t size) noexcept;
bool ReadNarrowString(std::uintptr_t address, std::string& result);
bool ReadWideString(std::uintptr_t address, std::string& result);
std::uint64_t HashCatId(std::uint64_t value) noexcept;
bool ValidateGameBuild(std::uintptr_t image, std::string& reason);
RoomSnapshot ReadFocusedRoom(std::uintptr_t image, bool include_cats, bool all_cats = false,
    const std::array<std::uint64_t,2>* requested_ids = nullptr);
RoomSnapshot ReadParentCats(std::uintptr_t image, const std::array<std::uint64_t,2>& ids,
    const std::array<std::string,2>& known_names = {});
bool ComponentHasType(std::uintptr_t component, std::uintptr_t image, const char* type);
std::uintptr_t FindCachedCatData(std::uintptr_t image, std::uint64_t id);
using NativeCatLoader = std::uintptr_t (*)(std::uintptr_t manager, std::uint64_t id);
void ConfigureParentCatLoader(NativeCatLoader loader);
using NativeRoomEffectsReader = std::uintptr_t (*)(std::uintptr_t room);
void ConfigureRoomEffectsReader(NativeRoomEffectsReader reader);
bool ReadComfortEffect(std::uintptr_t effect_vector, double& comfort);
bool ReadPopulationComfort(RoomSnapshot& snapshot);
bool ReadParentIds(std::uintptr_t pedigree, std::uint64_t cat, std::array<std::uint64_t, 2>& parents);
CatFamily ReadCatFamily(std::uintptr_t manager, std::uint64_t cat, int game_day);
std::uintptr_t FindHouseHudRenderer(std::uintptr_t image, std::uintptr_t scene);
std::string RoomLabel(const std::string& room_id);
std::string LocalizedLocationLabel(const std::string& key, const std::string& old_label);
}
