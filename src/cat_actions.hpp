#pragma once
#include "game_reader.hpp"

namespace roomcats {
struct NpcChoice {
    int npc = -1;
    std::string label;
    bool enabled = false;
    std::string reason;
    bool transient = false; // A native transport/UI transition, not NPC rejection.
};
enum class CatActionKind { Inspect, Donate, Mark };
enum class CatActionStatus { Running, Complete, Failed };
struct CatAction {
    CatActionKind kind = CatActionKind::Inspect;
    int npc = -1, phase = 0;
    std::uintptr_t scene = 0, component = 0, source = 0;
    std::uint64_t scene_generation = 0, cat = 0, cat_generation = 0, started = 0;
    std::uintptr_t pipe = 0, map = 0;
    std::uint64_t pipe_generation = 0, map_generation = 0;
    std::string marker;
};
struct CatActionResult {
    bool success = false, hide_list = false;
    std::uint64_t cat = 0;
    std::string message;
    bool completed = false;
};
using QueryCatNpcs = std::vector<NpcChoice> (*)(const RoomSnapshot&, const Cat&);
using StepCatAction = CatActionStatus (*)(const RoomSnapshot&, CatAction&, std::string&);
void ConfigureCatActions(QueryCatNpcs query, StepCatAction step);
std::vector<NpcChoice> CatNpcChoices(const RoomSnapshot&, std::uint64_t cat);
bool QueueCatAction(const RoomSnapshot&, std::uint64_t cat, CatActionKind kind, int npc = -1, const std::string& marker = {});
bool HasPendingCatAction();
bool CatActionNeedsCats();
void ProcessCatAction(const RoomSnapshot&, std::uint64_t now);
bool TakeCatActionResult(CatActionResult& result);
void CancelCatAction();
}
