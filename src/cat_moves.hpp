#pragma once
#include "game_reader.hpp"

namespace roomcats {
struct MoveChoice {
    CatLocation destination;
    bool enabled = false;
    bool current = false;
    std::string reason;
};
struct MoveResult {
    bool success = false;
    std::uint64_t cat = 0;
    std::string message;
};
using CheckCatMove = bool (*)(const RoomSnapshot&, const Cat&, const CatLocation&, std::string& reason);
using PerformCatMove = bool (*)(const RoomSnapshot&, const Cat&, const CatLocation&, std::string& reason);
void ConfigureCatMoves(CheckCatMove check, PerformCatMove perform);
std::vector<MoveChoice> CatMoveChoices(const RoomSnapshot& snapshot, std::uint64_t cat);
bool QueueCatMove(const RoomSnapshot& snapshot, std::uint64_t cat, const std::string& destination);
bool HasPendingCatMove();
void ProcessCatMove(const RoomSnapshot& fresh);
bool TakeCatMoveResult(MoveResult& result);
void CancelCatMove();
}
