#include "../src/cat_moves.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0, moves = 0;
bool allowed = true, succeeds = true, throws = false;
bool Check(const roomcats::RoomSnapshot&, const roomcats::Cat&, const roomcats::CatLocation&, std::string& why) {
    ++checks;
    if (!allowed) why = "箱子已满";
    return allowed;
}
bool Move(const roomcats::RoomSnapshot&, const roomcats::Cat& cat, const roomcats::CatLocation& where, std::string&) {
    ++moves;
    assert(cat.id == 42 && where.key == "box");
    if (throws) throw std::runtime_error("simulated backend failure");
    return succeeds;
}
}

int main() {
    using namespace roomcats;
    RoomSnapshot s;
    s.in_house = s.valid = true; s.scene = 123; s.generation = 7;
    s.locations = {{"room", "房间", 1000, 1, LocationKind::Room}, {"box", "箱子", 2000, 2, LocationKind::Box}};
    Cat cat; cat.id = 42; cat.name = "Example"; cat.location_key = "room"; s.cats.push_back(cat);
    ConfigureCatMoves(Check, Move);
    auto choices = CatMoveChoices(s, 42);
    assert(choices.size() == 2 && choices[0].current && !choices[0].enabled && choices[1].enabled);
    assert(moves == 0); // Opening/hovering the selector is read-only.
    assert(!QueueCatMove(s, 42, "room") && !QueueCatMove(s, 999, "box") && !QueueCatMove(s, 42, "missing"));
    assert(QueueCatMove(s, 42, "box") && HasPendingCatMove());
    assert(!QueueCatMove(s, 42, "box") && moves == 0);
    ProcessCatMove(s);
    MoveResult result;
    assert(TakeCatMoveResult(result) && result.success && result.message == "已移至 箱子" && moves == 1);
    ProcessCatMove(s); assert(moves == 1 && !HasPendingCatMove());
    for (int change = 0; change < 5; ++change) {
        auto changed = s;
        assert(QueueCatMove(s, 42, "box"));
        if (change == 0) changed.generation++;
        if (change == 1) changed.locations[1].generation++;
        if (change == 2) changed.cats.clear();
        if (change == 3) changed.cats[0].location_key = "elsewhere";
        if (change == 4) changed.in_house = false;
        ProcessCatMove(changed);
        assert(TakeCatMoveResult(result) && !result.success && moves == 1);
    }
    assert(QueueCatMove(s, 42, "box")); allowed = false;
    ProcessCatMove(s);
    assert(TakeCatMoveResult(result) && !result.success && result.message == "箱子已满" && moves == 1);
    assert(!CatMoveChoices(s, 42)[1].enabled);
    allowed = true; succeeds = false;
    assert(QueueCatMove(s, 42, "box")); ProcessCatMove(s);
    assert(TakeCatMoveResult(result) && !result.success && moves == 2);
    throws = true;
    assert(QueueCatMove(s, 42, "box")); ProcessCatMove(s);
    assert(TakeCatMoveResult(result) && !result.success && !HasPendingCatMove() && moves == 3);
    assert(QueueCatMove(s, 42, "box")); CancelCatMove(); ProcessCatMove(s);
    assert(moves == 3 && TakeCatMoveResult(result) && !result.success);
    ConfigureCatMoves(nullptr, nullptr);
    std::cout << "Move requests passed: previews never move, one dispatch per click, box eligibility, stale scene/cat/target rejection, cancellation and failed backend recovery.\n";
}
