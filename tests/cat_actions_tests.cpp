#include "../src/cat_actions.hpp"
#include <cassert>
#include <iostream>

namespace {
int queries = 0, starts = 0, advances = 0;
bool accepted = true, finish = false, cancel_during_step = false;
std::vector<roomcats::NpcChoice> Query(const roomcats::RoomSnapshot&, const roomcats::Cat&) {
    ++queries;
    return {{0, "Unlocked", accepted, ""}, {1, "Unavailable", false, "Reason"}};
}
roomcats::CatActionStatus Step(const roomcats::RoomSnapshot&, roomcats::CatAction& action, std::string& message) {
    using namespace roomcats;
    if (action.phase == 0) {
        ++starts;
        if (cancel_during_step) CancelCatAction();
        if (action.kind == CatActionKind::Inspect) { message = "Opened"; return CatActionStatus::Complete; }
        if (!accepted) return CatActionStatus::Failed;
        action.phase = 1; message = "Waiting";
    } else ++advances;
    return finish ? CatActionStatus::Complete : CatActionStatus::Running;
}
}

int main() {
    using namespace roomcats;
    ConfigureCatActions(Query, Step);
    RoomSnapshot snapshot;
    snapshot.valid = snapshot.in_house = true;
    snapshot.scene = 100; snapshot.generation = 5;
    Cat cat;
    cat.id = 42; cat.component = 200; cat.generation = 6; cat.location = 300;
    snapshot.cats.push_back(cat);
    CatActionResult result;
    for (int i = 0; i < 10; ++i) assert(CatNpcChoices(snapshot, 42).size() == 2);
    assert(starts == 0 && advances == 0 && !HasPendingCatAction());
    assert(!QueueCatAction(snapshot, 42, CatActionKind::Donate, 1));
    assert(!QueueCatAction(snapshot, 42, CatActionKind::Donate, 5)); // Locked/absent NPC.
    assert(QueueCatAction(snapshot, 42, CatActionKind::Donate, 0));
    assert(!QueueCatAction(snapshot, 42, CatActionKind::Inspect));
    assert(CatActionNeedsCats());
    ProcessCatAction(snapshot, 100);
    assert(TakeCatActionResult(result) && !result.hide_list && result.success);
    assert(starts == 1 && HasPendingCatAction() && !CatActionNeedsCats());
    auto fast = snapshot;
    fast.cats.clear(); // Animation waits do not rebuild all cat details.
    for (int i = 0; i < 8; ++i) ProcessCatAction(fast, 200+i);
    assert(starts == 1 && advances == 8 && !TakeCatActionResult(result));
    finish = true;
    ProcessCatAction(fast, 300);
    assert(TakeCatActionResult(result) && result.success && !result.hide_list && !HasPendingCatAction());
    finish = false;
    assert(QueueCatAction(snapshot, 42, CatActionKind::Inspect));
    ProcessCatAction(snapshot, 400);
    assert(TakeCatActionResult(result) && !result.hide_list && result.message == "Opened");
    for (int changed = 0; changed < 5; ++changed) {
        assert(QueueCatAction(snapshot, 42, CatActionKind::Donate, 0));
        auto stale = snapshot;
        if (changed == 0) stale.generation++;
        if (changed == 1) stale.cats[0].generation++;
        if (changed == 2) stale.cats[0].location++;
        if (changed == 3) stale.cats[0].component++;
        if (changed == 4) stale.cats.clear();
        ProcessCatAction(stale, 500);
        assert(TakeCatActionResult(result) && !result.success && !result.hide_list);
    }
    assert(starts == 2);
    assert(QueueCatAction(snapshot, 42, CatActionKind::Donate, 0));
    accepted = false; // Eligibility is rechecked after the user's click.
    ProcessCatAction(snapshot, 600);
    assert(TakeCatActionResult(result) && !result.success && !HasPendingCatAction());
    accepted = true;
    assert(QueueCatAction(snapshot, 42, CatActionKind::Donate, 0));
    ProcessCatAction(snapshot, 700);
    TakeCatActionResult(result);
    ProcessCatAction(fast, 30701);
    assert(TakeCatActionResult(result) && !result.success && !HasPendingCatAction());
    cancel_during_step = true;
    assert(QueueCatAction(snapshot, 42, CatActionKind::Donate, 0));
    ProcessCatAction(snapshot, 40000);
    assert(!HasPendingCatAction()); // Reentrant scene cancellation cannot resurrect a request.
    cancel_during_step = false;
    assert(QueueCatAction(snapshot, 42, CatActionKind::Donate, 0));
    ProcessCatAction(snapshot, 41000);
    TakeCatActionResult(result);
    CancelCatAction();
    assert(TakeCatActionResult(result) && !result.success);
    ConfigureCatActions(nullptr, nullptr);
    std::cout << "Cat actions passed: read-only previews, disabled/locked choices, one dispatch, native handoff waits, stale identities, eligibility changes, timeout and cancellation.\n";
}
